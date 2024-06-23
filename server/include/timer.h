#ifndef __TIMER__
#define __TIMER__

#include "hashset.h"

#define WHEEL_SIZE 3600

typedef struct {
    HashSet** slots;
    int curr_idx;
    int size;
} HashedWheelTimer;

HashedWheelTimer* hwtCreate(int size);
void hwtDestroy(HashedWheelTimer* timer);
// 返回新的slot_idx
int hwtUpdate(HashedWheelTimer* timer, int connfd, int old_slot_idx);
void hwtClear(HashedWheelTimer* timer);

#endif
