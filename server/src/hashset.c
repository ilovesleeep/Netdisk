#include "../include/hashset.h"

#define DEFAULT_CAPACITY 13
#define LOAD_FACTOR 0.75

HashSet* hashsetCreate(void) {
    HashSet* set = (HashSet*)calloc(1, sizeof(HashSet));
    if (!set) {
        perror("Failed to allocate memory for HashSet");
        exit(EXIT_FAILURE);
    }

    set->buckets = (SetNode**)calloc(DEFAULT_CAPACITY, sizeof(SetNode*));
    if (!set->buckets) {
        free(set);
        perror("Failed to allocate memory for HashSet buckets");
        exit(EXIT_FAILURE);
    }
    set->size = 0;
    set->capacity = DEFAULT_CAPACITY;
    set->hashseed = time(NULL);

    return set;
}

void hashsetDestroy(HashSet* set) {
    for (int i = 0; i < set->capacity; ++i) {
        SetNode* curr = set->buckets[i];  // 链表头
        while (curr) {
            SetNode* next = curr->next;
            free(curr);
            curr = next;
        }
    }
    free(set->buckets);
    free(set);
}

// murmurhash2
static uint32_t hashS(const void* key, int len, uint32_t seed) {
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    uint32_t h = seed ^ len;
    const unsigned char* data = (const unsigned char*)key;

    while (len >= 4) {
        // uint32_t k = *(uint32_t*)data;
        uint32_t k;
        memcpy(&k, data, sizeof(uint32_t));

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    switch (len) {
        case 3:
            h ^= data[2] << 16;
        case 2:
            h ^= data[1] << 8;
        case 1:
            h ^= data[0];
            h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

// quickPow
static long long quickPow(long long base, long long exponent, long long mod) {
    long long result = 1;
    base = base % mod;  // 在循环开始前进行一次取模

    while (exponent > 0) {
        // 如果 exponent 是奇数，将 base 乘到结果上
        if (exponent % 2 == 1) {
            result = (result * base) % mod;
        }
        // 将 exponent 减半
        exponent = exponent >> 1;
        // base 自身平方
        base = (base * base) % mod;
    }

    return result;
}

// Miller-Rabin
static bool millerRabin(int n) {
    srand((unsigned)time(NULL));
    int test_time = 8;
    if (n < 3 || n % 2 == 0) return n == 2;
    int u = n - 1, t = 0;
    while (u % 2 == 0) u /= 2, ++t;
    // test_time 为测试次数，建议设为不小于 8
    // 的整数以保证正确率，但也不宜过大，否则会影响效率
    for (int i = 0; i < test_time; ++i) {
        int a = rand() % (n - 2) + 2, v = quickPow(a, u, n);
        if (v == 1) continue;
        int s;
        for (s = 0; s < t; ++s) {
            if (v == n - 1) break;  // 得到平凡平方根 n-1，通过此轮测试
            v = (long long)v * v % n;
        }
        // 如果找到了非平凡平方根，则会由于无法提前 break; 而运行到 s == t
        // 如果 Fermat 素性测试无法通过，则一直运行到 s == t 前 v 都不会等于 -1
        if (s == t) return 0;
    }
    return 1;
}

static int findFirstPrime(int num) {
    while (1) {
        if (millerRabin(num)) break;
        num++;
    }
    return num;
}

static void hashsetResize(HashSet* set) {
    SetNode** old_buckets = set->buckets;
    int old_capacity = set->capacity;

    set->capacity = findFirstPrime(set->capacity + DEFAULT_CAPACITY);
    set->buckets = calloc(set->capacity, sizeof(SetNode*));
    set->size = 0;

    for (int i = 0; i < old_capacity; ++i) {
        SetNode* curr = old_buckets[i];
        while (curr) {
            SetNode* next = curr->next;
            hashsetInsert(set, curr->key);
            free(curr);
            curr = next;
        }
    }

    free(old_buckets);
}

void hashsetInsert(HashSet* set, int key) {
    if ((float)set->size / set->capacity >= LOAD_FACTOR) {
        hashsetResize(set);
    }

    int idx = hashS(&key, sizeof(key), set->hashseed) % set->capacity;
    SetNode* curr = set->buckets[idx];

    // 是否存在
    while (curr) {
        if (curr->key == key) {
            return;
        }
        curr = curr->next;
    }  // 不存在

    SetNode* newSetnode = (SetNode*)malloc(sizeof(SetNode));
    newSetnode->key = key;
    newSetnode->next = set->buckets[idx];  // 头插法
    set->buckets[idx] = newSetnode;

    set->size++;
    return;
}

// 1 找到，0 没找到
int hashsetSearch(HashSet* set, int key) {
    int idx = hashS(&key, sizeof(key), set->hashseed) % set->capacity;
    SetNode* curr = set->buckets[idx];
    while (curr) {
        if (curr->key == key) {
            return 1;
        }
        curr = curr->next;
    }

    return 0;
}

void hashsetDelete(HashSet* set, int key) {
    int idx = hashS(&key, sizeof(key), set->hashseed) % set->capacity;
    SetNode* pre = NULL;
    SetNode* curr = set->buckets[idx];
    while (curr) {
        if (curr->key == key) {
            if (pre == NULL) {
                set->buckets[idx] = curr->next;  // curr是第一个
            } else {
                pre->next = curr->next;
            }
            free(curr);
            set->size--;
            return;
        }
        pre = curr;
        curr = curr->next;
    }
}

void hashsetClear(HashSet* set) {
    if (set->size == 0) return;
    for (int i = 0; i < set->capacity; ++i) {
        SetNode* curr = set->buckets[i];
        while (curr != NULL) {
            SetNode* temp = curr;
            curr = curr->next;
            log_info("connection %d timeout, kick it out", temp->key);
            close(temp->key);
            free(temp);
        }
        set->buckets[i] = NULL;
    }
    set->size = 0;
}
