#include "hashmap.h"

#define DEFAULT_CAPACITY 13
#define LOAD_FACTOR 0.75

HashMap* hashmapCreate(void) {
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    if (!map) {
        perror("Failed to allocate memory for HashMap");
        exit(EXIT_FAILURE);
    }

    map->table = (Node**)calloc(DEFAULT_CAPACITY, sizeof(Node*));
    if (!map->table) {
        free(map);
        perror("Failed to allocate memory for HashMap table");
        exit(EXIT_FAILURE);
    }
    map->size = 0;
    map->capacity = DEFAULT_CAPACITY;
    map->hashseed = time(NULL);

    return map;
}

void hashmapDestroy(HashMap* map) {
    for (int i = 0; i < map->capacity; ++i) {
        Node* curr = map->table[i]; // 链表头
        while (curr) {
            Node* next = curr->next;
            free(curr);
            curr = next;
        }
    }
    free(map->table);
    free(map);
}

// murmurhash2
static uint32_t hash(const void* key, int len, uint32_t seed) {
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

    switch (len)
    {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
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
        if(millerRabin(num)) break;
        num++;
    }
    return num;
}



static void hashmapResize(HashMap* map) {
    Node** old_table = map->table;
    int old_capacity = map->capacity;

    map->capacity = findFirstPrime(map->capacity + DEFAULT_CAPACITY);
    map->table = calloc(map->capacity, sizeof(Node*));
    map->size = 0;

    for (int i = 0; i < old_capacity; ++i) {
        Node* curr= old_table[i];
        while (curr) {
            Node* next = curr->next;
            hashmapInsert(map, curr->key, curr->val);
            free(curr);
            curr = next;
        }
    }

    free(old_table);
}

void hashmapInsert(HashMap* map, int key, int val) {
    if ((float)map->size / map->capacity >= LOAD_FACTOR) {
        hashmapResize(map);
    }

    int idx = hash(&key, sizeof(key), map->hashseed) % map->capacity;
    Node* curr = map->table[idx];
    while (curr) {
        if (curr->key == key) {
            curr->val = val;
            return;
        }
        curr = curr->next;
    }

    Node* newnode = (Node*)malloc(sizeof(Node));
    newnode->key = key;
    newnode->val = val;
    newnode->next = map->table[idx]; // 头插法
    map->table[idx] = newnode;

    map->size++;
    return;
}

int hashmapSearch(HashMap* map, int key) {
    int idx = hash(&key, sizeof(key), map->hashseed) % map->capacity;
    Node* curr = map->table[idx];
    while (curr) {
        if (curr->key == key) {
            return curr->val;
        }
        curr = curr->next;
    }

    return -1; 
}


void hashmapDelete(HashMap* map, int key) {
    int idx = hash(&key, sizeof(key), map->hashseed) % map->capacity;
    Node* pre = NULL;
    Node* curr = map->table[idx];
    while (curr) {
        if(curr->key == key) {
            if (pre == NULL) {
                map->table[idx] = curr->next; // curr是第一个
            } else {
                pre->next = curr->next;
            }
            free(curr);
            map->size--;
            return;
        }
        pre = curr;
        curr = curr->next;
    }
}
