#ifndef __HASH_SET__
#define __HASH_SET__

#include <head.h>
#include <stdint.h>

typedef struct setnode {
    int key;
    struct setnode* next;
} SetNode;

typedef struct {
    SetNode** buckets;
    int size;
    int capacity;
    uint32_t hashseed;
} HashSet;

HashSet* hashsetCreate(void);
void hashsetDestroy(HashSet* set);
void hashsetInsert(HashSet* set, int key);
int hashsetSearch(HashSet* set, int Key);
void hashsetDelete(HashSet* set, int key);
void hashsetClear(HashSet* set);

#endif