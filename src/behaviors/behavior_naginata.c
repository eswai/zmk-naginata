/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_naginata

#include <zephyr/device.h>
#include <zephyr/settings/settings.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>

#include <zmk_naginata/nglist.h>
#include <zmk_naginata/nglistarray.h>
#include <zmk_naginata/naginata_func.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define NONE 0

// 薙刀式

// 31キーを32bitの各ビットに割り当てる
#define B_Q (1UL << 0)
#define B_W (1UL << 1)
#define B_E (1UL << 2)
#define B_R (1UL << 3)
#define B_T (1UL << 4)

#define B_Y (1UL << 5)
#define B_U (1UL << 6)
#define B_I (1UL << 7)
#define B_O (1UL << 8)
#define B_P (1UL << 9)

#define B_A (1UL << 10)
#define B_S (1UL << 11)
#define B_D (1UL << 12)
#define B_F (1UL << 13)
#define B_G (1UL << 14)

#define B_H (1UL << 15)
#define B_J (1UL << 16)
#define B_K (1UL << 17)
#define B_L (1UL << 18)
#define B_SEMI (1UL << 19)

#define B_Z (1UL << 20)
#define B_X (1UL << 21)
#define B_C (1UL << 22)
#define B_V (1UL << 23)
#define B_B (1UL << 24)

#define B_N (1UL << 25)
#define B_M (1UL << 26)
#define B_COMMA (1UL << 27)
#define B_DOT (1UL << 28)
#define B_SLASH (1UL << 29)

#define B_SPACE (1UL << 30)

static NGListArray nginput;
static uint32_t pressed_keys = 0UL; // 押しているキーのビットをたてる
static int8_t n_pressed_keys = 0;   // 押しているキーの数

extern user_config_t naginata_config;

#if IS_ENABLED(CONFIG_NAGINATA_PERSISTENT_STATE)
static void naginata_save_work_handler(struct k_work *work) {
    settings_save_one("naginata/state", &naginata_config, sizeof(naginata_config));
    LOG_DBG("Saved Naginata state: %d, %d", naginata_config.os, naginata_config.tategaki);
}

static struct k_work_delayable naginata_save_work;

static int naginata_settings_load_cb(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    if (settings_name_steq(name, "state", &next) && !next) {
        if (len != sizeof(naginata_config)) {
            return -EINVAL;
        }

        int rc = read_cb(cb_arg, &naginata_config, sizeof(naginata_config));
        if (rc >= 0) {
            LOG_INF("Loaded Naginata state: %d, %d", naginata_config.os, naginata_config.tategaki);
        }
        return MIN(rc, 0);
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(naginata, "naginata", NULL, naginata_settings_load_cb, NULL, NULL);
#endif // IS_ENABLED(CONFIG_NAGINATA_PERSISTENT_STATE)

static const uint32_t ng_key[] = {
    [A - A] = B_A,     [B - A] = B_B,         [C - A] = B_C,         [D - A] = B_D,
    [E - A] = B_E,     [F - A] = B_F,         [G - A] = B_G,         [H - A] = B_H,
    [I - A] = B_I,     [J - A] = B_J,         [K - A] = B_K,         [L - A] = B_L,
    [M - A] = B_M,     [N - A] = B_N,         [O - A] = B_O,         [P - A] = B_P,
    [Q - A] = B_Q,     [R - A] = B_R,         [S - A] = B_S,         [T - A] = B_T,
    [U - A] = B_U,     [V - A] = B_V,         [W - A] = B_W,         [X - A] = B_X,
    [Y - A] = B_Y,     [Z - A] = B_Z,         [SEMI - A] = B_SEMI,   [COMMA - A] = B_COMMA,
    [DOT - A] = B_DOT, [SLASH - A] = B_SLASH, [SPACE - A] = B_SPACE, [ENTER - A] = B_SPACE,
};

#define KANA_MAX_LEN 6

// カナ変換テーブル
typedef struct {
    uint32_t shift;
    uint32_t douji;
    uint32_t kana[KANA_MAX_LEN];
    void (*func)(void);
} naginata_kanamap;

static const naginata_kanamap ngdickana[] = {
    // 清音
    {.shift = NONE    , .douji = B_J            , .kana = {A, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // あ
    {.shift = NONE    , .douji = B_K            , .kana = {I, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // い
    {.shift = NONE    , .douji = B_L            , .kana = {U, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // う
    {.shift = B_SPACE , .douji = B_O            , .kana = {E, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // え
    {.shift = B_SPACE , .douji = B_N            , .kana = {O, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // お
    {.shift = NONE    , .douji = B_F            , .kana = {K, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // か
    {.shift = NONE    , .douji = B_W            , .kana = {K, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // き
    {.shift = NONE    , .douji = B_H            , .kana = {K, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // く
    {.shift = NONE    , .douji = B_S            , .kana = {K, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // け
    {.shift = NONE    , .douji = B_V            , .kana = {K, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // こ
    {.shift = B_SPACE , .douji = B_U            , .kana = {S, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // さ
    {.shift = NONE    , .douji = B_R            , .kana = {S, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // し
    {.shift = NONE    , .douji = B_O            , .kana = {S, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // す
    {.shift = B_SPACE , .douji = B_A            , .kana = {S, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // せ
    {.shift = NONE    , .douji = B_B            , .kana = {S, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // そ
    {.shift = NONE    , .douji = B_N            , .kana = {T, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // た
    {.shift = B_SPACE , .douji = B_G            , .kana = {T, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ち
    {.shift = B_SPACE , .douji = B_L            , .kana = {T, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // つ
    {.shift = NONE    , .douji = B_E            , .kana = {T, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // て
    {.shift = NONE    , .douji = B_D            , .kana = {T, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // と
    {.shift = NONE    , .douji = B_M            , .kana = {N, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // な
    {.shift = B_SPACE , .douji = B_D            , .kana = {N, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // に
    {.shift = B_SPACE , .douji = B_B            , .kana = {N, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぬ
    {.shift = B_SPACE , .douji = B_R            , .kana = {N, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ね
    {.shift = B_SPACE , .douji = B_J            , .kana = {N, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // の
    {.shift = NONE    , .douji = B_C            , .kana = {H, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // は
    {.shift = NONE    , .douji = B_X            , .kana = {H, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ひ
    {.shift = B_SPACE , .douji = B_X            , .kana = {H, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ひ
    {.shift = B_SPACE , .douji = B_SEMI         , .kana = {H, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふ
    {.shift = NONE    , .douji = B_P            , .kana = {H, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // へ
    {.shift = NONE    , .douji = B_Z            , .kana = {H, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ほ
    {.shift = B_SPACE , .douji = B_Z            , .kana = {H, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ほ
    {.shift = B_SPACE , .douji = B_F            , .kana = {M, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ま
    {.shift = B_SPACE , .douji = B_S            , .kana = {M, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // み
    {.shift = B_SPACE , .douji = B_COMMA        , .kana = {M, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // む
    {.shift = B_SPACE , .douji = B_W            , .kana = {M, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // め
    {.shift = B_SPACE , .douji = B_K            , .kana = {M, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // も
    {.shift = B_SPACE , .douji = B_H            , .kana = {Y, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // や
    {.shift = B_SPACE , .douji = B_P            , .kana = {Y, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゆ
    {.shift = B_SPACE , .douji = B_I            , .kana = {Y, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // よ
    {.shift = NONE    , .douji = B_DOT          , .kana = {R, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ら
    {.shift = B_SPACE , .douji = B_E            , .kana = {R, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // り
    {.shift = NONE    , .douji = B_I            , .kana = {R, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // る
    {.shift = NONE    , .douji = B_SLASH        , .kana = {R, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // れ
    {.shift = B_SPACE , .douji = B_SLASH        , .kana = {R, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // れ
    {.shift = NONE    , .douji = B_A            , .kana = {R, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ろ
    {.shift = B_SPACE , .douji = B_DOT          , .kana = {W, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // わ
    {.shift = B_SPACE , .douji = B_C            , .kana = {W, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // を
    {.shift = NONE    , .douji = B_COMMA        , .kana = {N, N, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ん
    {.shift = NONE    , .douji = B_SEMI         , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // ー

    // 濁音
    {.shift = NONE    , .douji = B_J|B_F        , .kana = {G, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // が
    {.shift = NONE    , .douji = B_J|B_W        , .kana = {G, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぎ
    {.shift = 0UL     , .douji = B_F|B_H        , .kana = {G, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぐ
    {.shift = NONE    , .douji = B_J|B_S        , .kana = {G, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // げ
    {.shift = NONE    , .douji = B_J|B_V        , .kana = {G, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ご
    {.shift = 0UL     , .douji = B_F|B_U        , .kana = {Z, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ざ
    {.shift = NONE    , .douji = B_J|B_R        , .kana = {Z, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じ
    {.shift = 0UL     , .douji = B_F|B_O        , .kana = {Z, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ず
    {.shift = NONE    , .douji = B_J|B_A        , .kana = {Z, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぜ
    {.shift = NONE    , .douji = B_J|B_B        , .kana = {Z, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぞ
    {.shift = 0UL     , .douji = B_F|B_N        , .kana = {D, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // だ
    {.shift = NONE    , .douji = B_J|B_G        , .kana = {D, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぢ
    {.shift = 0UL     , .douji = B_F|B_L        , .kana = {D, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // づ
    {.shift = NONE    , .douji = B_J|B_E        , .kana = {D, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // で
    {.shift = NONE    , .douji = B_J|B_D        , .kana = {D, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ど
    {.shift = NONE    , .douji = B_J|B_C        , .kana = {B, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ば
    {.shift = NONE    , .douji = B_J|B_X        , .kana = {B, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // び
    {.shift = 0UL     , .douji = B_F|B_SEMI     , .kana = {B, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぶ
    {.shift = 0UL     , .douji = B_F|B_P        , .kana = {B, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // べ
    {.shift = NONE    , .douji = B_J|B_Z        , .kana = {B, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぼ
    {.shift = 0UL     , .douji = B_F|B_L|B_SEMI , .kana = {V, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔ

    // 半濁音
    {.shift = NONE    , .douji = B_M|B_C        , .kana = {P, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぱ
    {.shift = NONE    , .douji = B_M|B_X        , .kana = {P, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぴ
    {.shift = NONE    , .douji = B_V|B_SEMI     , .kana = {P, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぷ
    {.shift = NONE    , .douji = B_V|B_P        , .kana = {P, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぺ
    {.shift = NONE    , .douji = B_M|B_Z        , .kana = {P, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぽ

    // 小書き
    {.shift = NONE    , .douji = B_Q|B_H        , .kana = {X, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ゃ
    {.shift = NONE    , .douji = B_Q|B_P        , .kana = {X, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ゅ
    {.shift = NONE    , .douji = B_Q|B_I        , .kana = {X, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ょ
    {.shift = NONE    , .douji = B_Q|B_J        , .kana = {X, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぁ
    {.shift = NONE    , .douji = B_Q|B_K        , .kana = {X, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぃ
    {.shift = NONE    , .douji = B_Q|B_L        , .kana = {X, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぅ
    {.shift = NONE    , .douji = B_Q|B_O        , .kana = {X, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぇ
    {.shift = NONE    , .douji = B_Q|B_N        , .kana = {X, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぉ
    {.shift = NONE    , .douji = B_Q|B_DOT      , .kana = {X, W, A, NONE, NONE, NONE      }, .func = nofunc }, // ゎ
    {.shift = NONE    , .douji = B_G            , .kana = {X, T, U, NONE, NONE, NONE      }, .func = nofunc }, // っ
    {.shift = NONE    , .douji = B_Q|B_S        , .kana = {X, K, E, NONE, NONE, NONE      }, .func = nofunc }, // ヶ
    {.shift = NONE    , .douji = B_Q|B_F        , .kana = {X, K, A, NONE, NONE, NONE      }, .func = nofunc }, // ヵ

    // 清音拗音 濁音拗音 半濁拗音
    {.shift = NONE    , .douji = B_R|B_H        , .kana = {S, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // しゃ
    {.shift = NONE    , .douji = B_R|B_P        , .kana = {S, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // しゅ
    {.shift = NONE    , .douji = B_R|B_I        , .kana = {S, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // しょ
    {.shift = NONE    , .douji = B_J|B_R|B_H    , .kana = {Z, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // じゃ
    {.shift = NONE    , .douji = B_J|B_R|B_P    , .kana = {Z, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // じゅ
    {.shift = NONE    , .douji = B_J|B_R|B_I    , .kana = {Z, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // じょ
    {.shift = NONE    , .douji = B_W|B_H        , .kana = {K, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // きゃ
    {.shift = NONE    , .douji = B_W|B_P        , .kana = {K, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // きゅ
    {.shift = NONE    , .douji = B_W|B_I        , .kana = {K, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // きょ
    {.shift = NONE    , .douji = B_J|B_W|B_H    , .kana = {G, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぎゃ
    {.shift = NONE    , .douji = B_J|B_W|B_P    , .kana = {G, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぎゅ
    {.shift = NONE    , .douji = B_J|B_W|B_I    , .kana = {G, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぎょ
    {.shift = NONE    , .douji = B_G|B_H        , .kana = {T, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ちゃ
    {.shift = NONE    , .douji = B_G|B_P        , .kana = {T, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ちゅ
    {.shift = NONE    , .douji = B_G|B_I        , .kana = {T, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ちょ
    {.shift = NONE    , .douji = B_J|B_G|B_H    , .kana = {D, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぢゃ
    {.shift = NONE    , .douji = B_J|B_G|B_P    , .kana = {D, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぢゅ
    {.shift = NONE    , .douji = B_J|B_G|B_I    , .kana = {D, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぢょ
    {.shift = NONE    , .douji = B_D|B_H        , .kana = {N, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // にゃ
    {.shift = NONE    , .douji = B_D|B_P        , .kana = {N, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // にゅ
    {.shift = NONE    , .douji = B_D|B_I        , .kana = {N, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // にょ
    {.shift = NONE    , .douji = B_X|B_H        , .kana = {H, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ひゃ
    {.shift = NONE    , .douji = B_X|B_P        , .kana = {H, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ひゅ
    {.shift = NONE    , .douji = B_X|B_I        , .kana = {H, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ひょ
    {.shift = NONE    , .douji = B_J|B_X|B_H    , .kana = {B, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // びゃ
    {.shift = NONE    , .douji = B_J|B_X|B_P    , .kana = {B, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // びゅ
    {.shift = NONE    , .douji = B_J|B_X|B_I    , .kana = {B, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // びょ
    {.shift = NONE    , .douji = B_M|B_X|B_H    , .kana = {P, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぴゃ
    {.shift = NONE    , .douji = B_M|B_X|B_P    , .kana = {P, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぴゅ
    {.shift = NONE    , .douji = B_M|B_X|B_I    , .kana = {P, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぴょ
    {.shift = NONE    , .douji = B_S|B_H        , .kana = {M, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // みゃ
    {.shift = NONE    , .douji = B_S|B_P        , .kana = {M, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // みゅ
    {.shift = NONE    , .douji = B_S|B_I        , .kana = {M, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // みょ
    {.shift = NONE    , .douji = B_E|B_H        , .kana = {R, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // りゃ
    {.shift = NONE    , .douji = B_E|B_P        , .kana = {R, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // りゅ
    {.shift = NONE    , .douji = B_E|B_I        , .kana = {R, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // りょ

    // 清音外来音 濁音外来音
    {.shift = NONE    , .douji = B_M|B_E|B_K    , .kana = {T, H, I, NONE, NONE, NONE      }, .func = nofunc }, // てぃ
    {.shift = NONE    , .douji = B_M|B_E|B_P    , .kana = {T, E, X, Y, U, NONE            }, .func = nofunc }, // てゅ
    {.shift = NONE    , .douji = B_J|B_E|B_K    , .kana = {D, H, I, NONE, NONE, NONE      }, .func = nofunc }, // でぃ
    {.shift = NONE    , .douji = B_J|B_E|B_P    , .kana = {D, H, U, NONE, NONE, NONE      }, .func = nofunc }, // でゅ
    {.shift = NONE    , .douji = B_M|B_D|B_L    , .kana = {T, O, X, U, NONE, NONE         }, .func = nofunc }, // とぅ
    {.shift = NONE    , .douji = B_J|B_D|B_L    , .kana = {D, O, X, U, NONE, NONE         }, .func = nofunc }, // どぅ
    {.shift = NONE    , .douji = B_M|B_R|B_O    , .kana = {S, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // しぇ
    {.shift = NONE    , .douji = B_M|B_G|B_O    , .kana = {T, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // ちぇ
    {.shift = NONE    , .douji = B_J|B_R|B_O    , .kana = {Z, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // じぇ
    {.shift = NONE    , .douji = B_J|B_G|B_O    , .kana = {D, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // ぢぇ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_J , .kana = {F, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぁ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_K , .kana = {F, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぃ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_O , .kana = {F, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぇ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_N , .kana = {F, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぉ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_P , .kana = {F, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ふゅ
    {.shift = NONE    , .douji = B_V|B_K|B_O    , .kana = {I, X, E, NONE, NONE, NONE      }, .func = nofunc }, // いぇ
    {.shift = NONE    , .douji = B_V|B_L|B_K    , .kana = {W, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぃ
    {.shift = NONE    , .douji = B_V|B_L|B_O    , .kana = {W, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぇ
    {.shift = NONE    , .douji = B_V|B_L|B_N    , .kana = {U, X, O, NONE, NONE, NONE      }, .func = nofunc }, // うぉ
    {.shift = NONE    , .douji = B_F|B_L|B_J    , .kana = {V, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぁ
    {.shift = NONE    , .douji = B_F|B_L|B_K    , .kana = {V, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぃ
    {.shift = NONE    , .douji = B_F|B_L|B_O    , .kana = {V, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぇ
    {.shift = NONE    , .douji = B_F|B_L|B_N    , .kana = {V, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぉ
    {.shift = NONE    , .douji = B_F|B_L|B_P    , .kana = {V, U, X, Y, U, NONE            }, .func = nofunc }, // ゔゅ
    {.shift = NONE    , .douji = B_V|B_H|B_J    , .kana = {K, U, X, A, NONE, NONE         }, .func = nofunc }, // くぁ
    {.shift = NONE    , .douji = B_V|B_H|B_K    , .kana = {K, U, X, I, NONE, NONE         }, .func = nofunc }, // くぃ
    {.shift = NONE    , .douji = B_V|B_H|B_O    , .kana = {K, U, X, E, NONE, NONE         }, .func = nofunc }, // くぇ
    {.shift = NONE    , .douji = B_V|B_H|B_N    , .kana = {K, U, X, O, NONE, NONE         }, .func = nofunc }, // くぉ
    {.shift = NONE    , .douji = B_V|B_H|B_DOT  , .kana = {K, U, X, W, A, NONE            }, .func = nofunc }, // くゎ
    {.shift = NONE    , .douji = B_F|B_H|B_J    , .kana = {G, U, X, A, NONE, NONE         }, .func = nofunc }, // ぐぁ
    {.shift = NONE    , .douji = B_F|B_H|B_K    , .kana = {G, U, X, I, NONE, NONE         }, .func = nofunc }, // ぐぃ
    {.shift = NONE    , .douji = B_F|B_H|B_O    , .kana = {G, U, X, E, NONE, NONE         }, .func = nofunc }, // ぐぇ
    {.shift = NONE    , .douji = B_F|B_H|B_N    , .kana = {G, U, X, O, NONE, NONE         }, .func = nofunc }, // ぐぉ
    {.shift = NONE    , .douji = B_F|B_H|B_DOT  , .kana = {G, U, X, W, A, NONE            }, .func = nofunc }, // ぐゎ
    {.shift = NONE    , .douji = B_V|B_L|B_J    , .kana = {T, S, A, NONE, NONE, NONE      }, .func = nofunc }, // つぁ
    
    // 追加
    {.shift = NONE    , .douji = B_SPACE        , .kana = {SPACE, NONE, NONE, NONE, NONE, NONE  }, .func = nofunc},
    {.shift = B_SPACE , .douji = B_V            , .kana = {COMMA, ENTER, NONE, NONE, NONE, NONE }, .func = nofunc},
    {.shift = NONE    , .douji = B_Q            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc},
    {.shift = B_SPACE , .douji = B_Q            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc},
    {.shift = B_SPACE , .douji = B_M            , .kana = {DOT, ENTER, NONE, NONE, NONE, NONE   }, .func = nofunc},
    {.shift = NONE    , .douji = B_U            , .kana = {BSPC, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc},

    {.shift = NONE    , .douji = B_V|B_M        , .kana = {ENTER, NONE, NONE, NONE, NONE, NONE  }, .func = nofunc}, // enter
    // {.shift = B_SPACE, .douji = B_V|B_M, .kana = {ENTER, NONE, NONE, NONE, NONE, NONE}, .func = nofunc}, // enter+シフト(連続シフト)

    {.shift = NONE    , .douji = B_T            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_T}, //
    {.shift = NONE    , .douji = B_Y            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_Y}, //
    {.shift = B_SPACE , .douji = B_T            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_ST}, //
    {.shift = B_SPACE , .douji = B_Y            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_SY}, //
    {.shift = NONE    , .douji = B_H|B_J        , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = naginata_on}, // 　かなオン
    // {.shift = NONE, .douji = B_F | B_G, .kana = {NONE, NONE, NONE, NONE, NONE, NONE}, .func = naginata_off}, // 　かなオフ

    // 編集モード
    {.shift = B_J|B_K    , .douji = B_Q     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKQ    }, // ^{End}
    {.shift = B_J|B_K    , .douji = B_W     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKW    }, // ／{改行}
    {.shift = B_J|B_K    , .douji = B_E     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKE    }, // /*ディ*/
    {.shift = B_J|B_K    , .douji = B_R     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKR    }, // ^s
    {.shift = B_J|B_K    , .douji = B_T     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKT    }, // ・
    {.shift = B_J|B_K    , .douji = B_A     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKA    }, // ……{改行}
    {.shift = B_J|B_K    , .douji = B_S     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKS    }, // 『{改行}
    {.shift = B_J|B_K    , .douji = B_D     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKD    }, // ？{改行}
    {.shift = B_J|B_K    , .douji = B_F     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKF    }, // 「{改行}
    {.shift = B_J|B_K    , .douji = B_G     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKG    }, // ({改行}
    {.shift = B_J|B_K    , .douji = B_Z     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKZ    }, // ――{改行}
    {.shift = B_J|B_K    , .douji = B_X     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKX    }, // 』{改行}
    {.shift = B_J|B_K    , .douji = B_C     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKC    }, // ！{改行}
    {.shift = B_J|B_K    , .douji = B_V     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKV    }, // 」{改行}
    {.shift = B_J|B_K    , .douji = B_B     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKB    }, // ){改行}
    {.shift = B_D|B_F    , .douji = B_Y     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFY    }, // {Home}
    {.shift = B_D|B_F    , .douji = B_U     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFU    }, // +{End}{BS}
    {.shift = B_D|B_F    , .douji = B_I     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFI    }, // {vk1Csc079}
    {.shift = B_D|B_F    , .douji = B_O     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFO    }, // {Del}
    {.shift = B_D|B_F    , .douji = B_P     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFP    }, // +{Esc 3}
    {.shift = B_D|B_F    , .douji = B_H     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFH    }, // {Enter}{End}
    {.shift = B_D|B_F    , .douji = B_J     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFJ    }, // {↑}
    {.shift = B_D|B_F    , .douji = B_K     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFK    }, // +{↑}
    {.shift = B_D|B_F    , .douji = B_L     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFL    }, // +{↑ 7}
    {.shift = B_D|B_F    , .douji = B_SEMI  , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFSCLN }, // ^i
    {.shift = B_D|B_F    , .douji = B_N     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFN    }, // {End}
    {.shift = B_D|B_F    , .douji = B_M     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFM    }, // {↓}
    {.shift = B_D|B_F    , .douji = B_COMMA , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFCOMM }, // +{↓}
    {.shift = B_D|B_F    , .douji = B_DOT   , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFDOT  }, // +{↓ 7}
    {.shift = B_D|B_F    , .douji = B_SLASH , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFSLSH }, // ^u
    {.shift = B_M|B_COMMA, .douji = B_Q     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCQ    }, // ｜{改行}
    {.shift = B_M|B_COMMA, .douji = B_W     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCW    }, // 　　　×　　　×　　　×{改行 2}
    {.shift = B_M|B_COMMA, .douji = B_E     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCE    }, // {Home}{→}{End}{Del 2}{←}
    {.shift = B_M|B_COMMA, .douji = B_R     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCR    }, // {Home}{改行}{Space 1}{←}
    {.shift = B_M|B_COMMA, .douji = B_T     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCT    }, // 〇{改行}
    {.shift = B_M|B_COMMA, .douji = B_A     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCA    }, // 《{改行}
    {.shift = B_M|B_COMMA, .douji = B_S     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCS    }, // 【{改行}
    {.shift = B_M|B_COMMA, .douji = B_D     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCD    }, // {Home}{→}{End}{Del 4}{←}
    {.shift = B_M|B_COMMA, .douji = B_F     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCF    }, // {Home}{改行}{Space 3}{←}
    {.shift = B_M|B_COMMA, .douji = B_G     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCG    }, // {Space 3}
    {.shift = B_M|B_COMMA, .douji = B_Z     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCZ    }, // 》{改行}
    {.shift = B_M|B_COMMA, .douji = B_X     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCX    }, // 】{改行}
    {.shift = B_M|B_COMMA, .douji = B_C     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCC    }, // 」{改行}{改行}
    {.shift = B_M|B_COMMA, .douji = B_V     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCV    }, // 」{改行}{改行}「{改行}
    {.shift = B_M|B_COMMA, .douji = B_B     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCB    }, // 」{改行}{改行}{Space}
    {.shift = B_C|B_V    , .douji = B_Y     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVY    }, // +{Home}
    {.shift = B_C|B_V    , .douji = B_U     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVU    }, // ^x
    {.shift = B_C|B_V    , .douji = B_I     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVI    }, // {vk1Csc079}
    {.shift = B_C|B_V    , .douji = B_O     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVO    }, // ^v
    {.shift = B_C|B_V    , .douji = B_P     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVP    }, // ^z
    {.shift = B_C|B_V    , .douji = B_H     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVH    }, // ^c
    {.shift = B_C|B_V    , .douji = B_J     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVJ    }, // {←}
    {.shift = B_C|B_V    , .douji = B_K     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVK    }, // {→}
    {.shift = B_C|B_V    , .douji = B_L     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVL    }, // +{← 7}
    {.shift = B_C|B_V    , .douji = B_SEMI  , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVSCLN }, // +{→ 7}
    {.shift = B_C|B_V    , .douji = B_N     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVN    }, // +{End}
    {.shift = B_C|B_V    , .douji = B_M     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVM    }, // +{←}
    {.shift = B_C|B_V    , .douji = B_COMMA , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVCOMM }, // +{→}
    {.shift = B_C|B_V    , .douji = B_DOT   , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVDOT  }, // {End}+{Home}
    {.shift = B_C|B_V    , .douji = B_SLASH , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVSLSH }, // ^y

};

// Helper function for counting matches/candidates
static int count_kana_entries(NGList *keys, bool exact_match) {
  if (keys->size == 0) return 0;
  // 変換テーブルの最大同時押しは3キー。4キー以上に一致するエントリは存在しない。
  // 連続シフト処理でリストが4キー以上に伸びる場合があるため、ここで弾く。
  if (keys->size > 3) return 0;

  int count = 0;
  uint32_t keyset0 = 0UL, keyset1 = 0UL, keyset2 = 0UL;
  
  // keysetを配列にしたらバイナリサイズが増えた
  switch (keys->size) {
    case 1:
      keyset0 = ng_key[keys->elements[0] - A];
      break;
    case 2:
      keyset0 = ng_key[keys->elements[0] - A];
      keyset1 = ng_key[keys->elements[1] - A];
      break;
    default:
      keyset0 = ng_key[keys->elements[0] - A];
      keyset1 = ng_key[keys->elements[1] - A];
      keyset2 = ng_key[keys->elements[2] - A];
      break;
  }

  for (int i = 0; i < sizeof ngdickana / sizeof ngdickana[0]; i++) {
    bool matches = false;

    switch (keys->size) {
      case 1:
        if (exact_match) {
          matches = (ngdickana[i].shift == keyset0) || 
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == keyset0);
        } else {
          matches = ((ngdickana[i].shift & keyset0) == keyset0) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & keyset0) == keyset0);
        }
        break;
      case 2:
        if (exact_match) {
          matches = (ngdickana[i].shift == (keyset0 | keyset1)) ||
                   (ngdickana[i].shift == keyset0 && ngdickana[i].douji == keyset1) ||
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == (keyset0 | keyset1));
        } else {
          matches = (ngdickana[i].shift == (keyset0 | keyset1)) ||
                   (ngdickana[i].shift == keyset0 && (ngdickana[i].douji & keyset1) == keyset1) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & (keyset0 | keyset1)) == (keyset0 | keyset1));
          // しぇ、ちぇ、など2キーで確定してはいけない
          if (matches && (ngdickana[i].shift | ngdickana[i].douji) != (keyset0 | keyset1)) {
            count = 2;
          }
        }
        break;
      default:
        if (exact_match) {
          matches = (ngdickana[i].shift == (keyset0 | keyset1) && ngdickana[i].douji == keyset2) ||
                   (ngdickana[i].shift == keyset0 && ngdickana[i].douji == (keyset1 | keyset2)) ||
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == (keyset0 | keyset1 | keyset2));
        } else {
          matches = (ngdickana[i].shift == (keyset0 | keyset1) && (ngdickana[i].douji & keyset2) == keyset2) ||
                   (ngdickana[i].shift == keyset0 && (ngdickana[i].douji & (keyset1 | keyset2)) == (keyset1 | keyset2)) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & (keyset0 | keyset1 | keyset2)) == (keyset0 | keyset1 | keyset2));
        }
        break;
    }

    if (matches) {
      count++;
      if (count > 1) break;
    }
  }

  return count;
}

int number_of_matches(NGList *keys) {
  return count_kana_entries(keys, true);
}

int number_of_candidates(NGList *keys) {
  return count_kana_entries(keys, false);
}

// キー入力を文字に変換して出力する
void ng_type(NGList *keys) {
    LOG_DBG(">NAGINATA NG_TYPE");

    if (keys->size == 0)
        return;

    int64_t ts = naginata_get_timestamp();

    if (keys->size == 1 && keys->elements[0] == ENTER) {
        LOG_DBG(" NAGINATA type keycode 0x%02X", ENTER);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, true, ts);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, false, ts);
        return;
    }

    uint32_t keyset = 0UL;
    for (int i = 0; i < keys->size; i++) {
        keyset |= ng_key[keys->elements[i] - A];
    }

    for (int i = 0; i < sizeof ngdickana / sizeof ngdickana[0]; i++) {
        if ((ngdickana[i].shift | ngdickana[i].douji) == keyset) {
            if (ngdickana[i].kana[0] != NONE) {
                for (int k = 0; k < KANA_MAX_LEN; k++) {
                    if (ngdickana[i].kana[k] == NONE)
                        break;
                    LOG_DBG(" NAGINATA type keycode 0x%02X", ngdickana[i].kana[k]);
                    raise_zmk_keycode_state_changed_from_encoded(ngdickana[i].kana[k], true,
                                                                 ts);
                    raise_zmk_keycode_state_changed_from_encoded(ngdickana[i].kana[k], false,
                                                                 ts);
                }
            } else {
                ngdickana[i].func();
            }
            LOG_DBG("<NAGINATA NG_TYPE");
            return;
        }
    }

    // 1キーで一致しなかった場合、これ以上分割できない。
    // ここで分割再帰すると同じ1キーを無限に再帰してスタックを溢れさせるため打ち切る。
    if (keys->size <= 1) {
        LOG_DBG("<NAGINATA NG_TYPE (no match, single key)");
        return;
    }

    // JIみたいにJIを含む同時押しはたくさんあるが、JIのみの同時押しがないとき
    // 最後の１キーを別に分けて変換する
    NGList a, b;
    initializeList(&a);
    initializeList(&b);
    for (int i = 0; i < keys->size - 1; i++) {
        addToList(&a, keys->elements[i]);
    }
    addToList(&b, keys->elements[keys->size - 1]);
    ng_type(&a);
    ng_type(&b);

    LOG_DBG("<NAGINATA NG_TYPE");
}

// nginputに入力を積む。満杯なら最古の入力を確定して空きを作ってから追加する。
// (戻り値を無視して黙ってキーを捨てるのを防ぐ)
static void push_input(NGList *e) {
    if (!addToListArray(&nginput, e)) {
        ng_type(&(nginput.elements[0]));
        removeFromListArrayAt(&nginput, 0);
        addToListArray(&nginput, e);
    }
}

// 薙刀式の入力処理
bool naginata_press(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    LOG_DBG(">NAGINATA PRESS");

    uint32_t keycode = binding->param1;

    switch (keycode) {
    case A ... Z:
    case SPACE:
    case ENTER:
    case DOT:
    case COMMA:
    case SLASH:
    case SEMI:
        n_pressed_keys++;
        pressed_keys |= ng_key[keycode - A]; // キーの重ね合わせ

        if (keycode == SPACE || keycode == ENTER) {
            NGList a;
            initializeList(&a);
            addToList(&a, keycode);
            push_input(&a);
        } else {
            NGList a;
            NGList b;
            if (nginput.size > 0) {
                copyList(&(nginput.elements[nginput.size - 1]), &a);
                copyList(&a, &b);
                addToList(&b, keycode);
            }

            // 前のキーとの同時押しの可能性があるなら前に足す
            // 同じキー連打を除外
            if (nginput.size > 0 && a.elements[a.size - 1] != keycode &&
                number_of_candidates(&b) > 0) {
                removeFromListArrayAt(&nginput, nginput.size - 1);
                addToListArray(&nginput, &b);
                // 前のキーと同時押しはない
            } else {
                // 連続シフトではない
                NGList e;
                initializeList(&e);
                addToList(&e, keycode);
                push_input(&e);
            }
        }

        // 連続シフト
        static const uint32_t rs[10][2] = {{D, F},     {C, V}, {J, K}, {M, COMMA}, {SPACE, 0},
                                     {ENTER, 0}, {F, 0}, {V, 0}, {J, 0},     {M, 0}};

        uint32_t keyset = 0UL;
        for (int i = 0; i < nginput.elements[nginput.size - 1].size; i++) {
            keyset |= ng_key[nginput.elements[nginput.size - 1].elements[i] - A];
        }
        for (int i = 0; i < 10; i++) {
            NGList rskc;
            initializeList(&rskc);
            addToList(&rskc, rs[i][0]);
            if (rs[i][1] > 0) {
                addToList(&rskc, rs[i][1]);
            }

            int c = includeList(&rskc, keycode);
            uint32_t brs = 0UL;
            for (int j = 0; j < rskc.size; j++) {
                brs |= ng_key[rskc.elements[j] - A];
            }

            NGList l = nginput.elements[nginput.size - 1];
            bool overflow = false;
            for (int j = 0; j < l.size; j++) {
                if (!addToList(&rskc, l.elements[j])) {
                    overflow = true;
                    break;
                }
            }
            if (overflow) {
                continue;
            }

            if (c < 0 && ((brs & pressed_keys) == brs) && (keyset & brs) != brs && number_of_matches(&rskc) > 0) {
                nginput.elements[nginput.size - 1] = rskc;
                break;
            }
        }

        if (nginput.size > 1 || number_of_candidates(&(nginput.elements[0])) == 1) {
            ng_type(&(nginput.elements[0]));
            removeFromListArrayAt(&nginput, 0);
        }
        break;
    }

    LOG_DBG("<NAGINATA PRESS");

    return true;
}

bool naginata_release(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    LOG_DBG(">NAGINATA RELEASE");

    uint32_t keycode = binding->param1;

    switch (keycode) {
    case A ... Z:
    case SPACE:
    case ENTER:
    case DOT:
    case COMMA:
    case SLASH:
    case SEMI:
        if (n_pressed_keys > 0)
            n_pressed_keys--;
        if (n_pressed_keys == 0)
            pressed_keys = 0UL;

        pressed_keys &= ~ng_key[keycode - A]; // キーの重ね合わせ

        if (pressed_keys == 0UL) {
            while (nginput.size > 0) {
                ng_type(&(nginput.elements[0]));
                removeFromListArrayAt(&nginput, 0);
            }
        } else {
            if (nginput.size > 0 && number_of_candidates(&(nginput.elements[0])) == 1) {
                ng_type(&(nginput.elements[0]));
                removeFromListArrayAt(&nginput, 0);
            }
        }
        break;
    }

    LOG_DBG("<NAGINATA RELEASE");

    return true;
}

// 薙刀式

static int behavior_naginata_init(const struct device *dev) {
    LOG_DBG("NAGINATA INIT");

    initializeListArray(&nginput);
    pressed_keys = 0UL;
    n_pressed_keys = 0;
#if defined(CONFIG_NAGINATA_DEFAULT_OS_WINDOWS)
    naginata_config.os = NG_WINDOWS;
#elif defined(CONFIG_NAGINATA_DEFAULT_OS_LINUX)
    naginata_config.os = NG_LINUX;
#else
    naginata_config.os = NG_MACOS;
#endif

#if IS_ENABLED(CONFIG_NAGINATA_PERSISTENT_STATE)
    k_work_init_delayable(&naginata_save_work, naginata_save_work_handler);
#endif
    return 0;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    // F15が押されたらnaginata_config.os=NG_WINDOWS
    {
#if IS_ENABLED(CONFIG_NAGINATA_PERSISTENT_STATE)
        uint8_t saved_os = naginata_config.os;
        bool saved_tategaki = naginata_config.tategaki;
#endif
        switch (binding->param1) {
            case F15:
                naginata_config.os = NG_WINDOWS;
                break;
            case F16:
                naginata_config.os = NG_MACOS;
                break;
            case F17:
                naginata_config.os = NG_LINUX;
                break;
            case F18:
                naginata_config.tategaki = true;
                break;
            case F19:
                naginata_config.tategaki = false;
                break;
        }
        switch (binding->param1) {
            case F15 ... F19:
#if IS_ENABLED(CONFIG_NAGINATA_PERSISTENT_STATE)
                if (naginata_config.os != saved_os || naginata_config.tategaki != saved_tategaki) {
                    // Debounce saving to flash to reduce wear
                    k_work_reschedule(&naginata_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
                }
#endif
                return ZMK_BEHAVIOR_OPAQUE;
        }
    }

    naginata_set_timestamp(event.timestamp);
    naginata_press(binding, event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    naginata_set_timestamp(event.timestamp);
    naginata_release(binding, event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_naginata_driver_api = {
    .binding_pressed = on_keymap_binding_pressed, .binding_released = on_keymap_binding_released};

#define KP_INST(n)                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_naginata_init, NULL, NULL, NULL, POST_KERNEL,              \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_naginata_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)
