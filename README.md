# ZMK 薙刀式

ZMK Firmwareで薙刀式かな入力を実現します。
薙刀式は大岡俊彦氏が考案されたかな入力方式です。

http://oookaworks.seesaa.net/article/456099128.html

## Github Actionsでbuildする場合

[zmk-config](https://zmk.dev/docs/customization)のwest.ymlに2か所を追加
```
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: eswai # ここを追加
      url-base: https://github.com/eswai
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-naginata # ここを追加
      remote: eswai
      revision: main
  self:
    path: config
```

config/boards/your_keyboard/your_keyboard.keymapに薙刀式のコンボとレイヤーを追加
```
#include <behaviors/naginata.dtsi>

/ {

    macros {
        ng_on: ng_on {
            compatible = "zmk,behavior-macro";
            #binding-cells = <0>;
            bindings
                = <&macro_tap &kp LANGUAGE_1 &kp INTERNATIONAL_4 &to 1>
                ;
        };
        ng_off: ng_off {
            compatible = "zmk,behavior-macro";
            #binding-cells = <0>;
            bindings
                = <&macro_tap &kp LANGUAGE_2 &kp INTERNATIONAL_5 &to 0>
                ;
        };
    };

    combos {
        compatible = "zmk,combos";
        combo_ng_on {
            timeout-ms = <300>;
            key-positions = <15 16>; // H, Jのキー位置に書き換えてください
            bindings = <&ng_on>;
            layers = <0>; // デフォルトレイヤーが0の前提
        };
        combo_ng_off {
            timeout-ms = <300>;
            key-positions = <13 14>; // F, Gのキー位置に書き換えてください
            bindings = <&ng_off>;
            layers = <0 1>; // デフォルトレイヤーが0、薙刀式レイヤーが1の前提
        };
    };


    keymap {
        compatible = "zmk,keymap";

        default_layer {
            bindings = <
            // +----------+----------+----------+----------+----------+----------+----------+----------+----------+----------
                &kp K      &kp D      &kp N      &kp F      &kp Q      &kp J      &kp BSPC   &kp R      &kp U      &kp P
                &kp W      &kp I      &kp S      &kp A      &kp G      &kp Y      &kp E      &kp T      &kp H      &kp B
                &kp Z      &kp X      &kp V      &kp C      &kp L      &kp M      &kp O      &kp COMMA  &kp DOT    &kp SLASH
                                      &kp LCMD   &mo LOWER  &mt LSHFT SPACE  &mt LSHFT ENTER  &mo RAISE  &kp RCTRL
                >;
        };

        // 薙刀式に必要な31キー(Enterもシフトキーにしたいときは32キー)を定義してください。
        // &kpの代わりに&ngを使います。通常はデフォルトレイヤーの次に配置してください(レイヤー番号1)。
        naginata_layer { 
            bindings = <
            // +----------+----------+----------+----------+----------+----------+----------+----------+----------+----------
                &ng Q      &ng W      &ng E      &ng R      &ng T      &ng Y      &ng U      &ng I      &ng O      &ng P
                &ng A      &ng S      &ng D      &ng F      &ng G      &ng H      &ng J      &ng K      &ng L      &ng SEMI
                &ng Z      &ng X      &ng C      &ng V      &ng B      &ng N      &ng M      &ng COMMA  &ng DOT    &ng SLASH
                                      &kp LSHIFT &mo LOWER  &ng SPACE  &ng ENTER  &mo RAISE  &kp RCTRL
                >;
        };
 
// 以下省略
```

## ローカルでbuildする場合の例

```
git clone https://github.com/eswai/zmk-naginata.git
cd zmk/app

rm -rf build
west build -b seeeduino_xiao_ble -- -DSHIELD=your_keyboard_left -DZMK_CONFIG="/Users/foo/zmk-config/config" -DZMK_EXTRA_MODULES="/Users/foo/zmk-naginata"
cp build/zephyr/zmk.uf2 ~/zmk_left.uf2

# 分割なら右手も
rm -rf build
west build -b seeeduino_xiao_ble -- -DSHIELD=your_keyboard_right -DZMK_CONFIG="/Users/foo/zmk-config/config" -DZMK_EXTRA_MODULES="/Users/foo/zmk-naginata"
cp build/zephyr/zmk.uf2 ~/zmk_right.uf2
```

## 編集モードへの対応
Windowsでは下の対応をすることによって編集モードで記号入力を行うことができます。
* [WinCompose](https://github.com/SamHocevar/wincompose) をインストール
* `&ng F15` を割り当てたキーを押す。60秒後に消えないように保存されます。

Macでは下の対応をすることによって編集モードで記号入力を行うことができます。

* Macの「Unicode 16進数入力」を入力ソースに追加する
* Karabiner-Elementsをインストール
* unicode_hex_input_switcher.jsonをKarabiner-Elementsで有効にする
* `&ng F16` を割り当てたキーを押す。60秒後に消えないように保存されます。
