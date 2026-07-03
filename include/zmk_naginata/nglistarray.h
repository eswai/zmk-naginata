#pragma once
#include <zmk_naginata/nglist.h>

typedef struct {
    NGList elements[LIST_SIZE];
    int size;
} NGListArray;

void initializeListArray(NGListArray *);
bool addToListArray(NGListArray *, NGList *);
bool removeFromListArrayAt(NGListArray *, int);
