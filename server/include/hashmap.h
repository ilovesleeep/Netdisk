#ifndef __HASH_TABLE__
#define __HASH_TABlE__
#include "head.h"
#include <stdint.h>

typedef struct node {
    int key;
    int val;
    struct node* next;
} Node;

typedef struct {
    Node** table; // 拉链法
    int size;
    int capacity;
    uint32_t hashseed; //unsigned int
} HashMap;

HashMap* hashmapCreate(void);
void hashmapDestroy(HashMap* map);

void hashmapInsert(HashMap* map, int key, int val);
int hashmapSearch(HashMap* map, int key);
void hashmapDelete(HashMap* map, int key);

#endif
