#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "map.h"

#define HT_LOAD    0.60
#define HT_DELETED LONG_MAX
#define HT_PROBE_1 1
#define HT_PROBE_3 3

MapIndex *MapIndexNew(long capacity) {
    long offset = (sizeof(long) * 2) + sizeof(long *);
    void *memory = malloc(offset +
                          (sizeof(MapIndex)) * (capacity * sizeof(long)));
    MapIndex *idx = memory;
    idx->len = 0;
    idx->capacity = capacity;
    idx->entries = memory + offset;
    return idx;
}

MapIndex *MapIndexReAlloc(MapIndex *idx) { // Reallocate the whole structure
    long new_capacity = idx->capacity * 4;
    MapIndex *new = MapIndexNew(new_capacity);
    memcpy(new->entries, idx->entries, idx->capacity * sizeof(long));
    new->capacity = new_capacity;
    new->len = idx->len;
    return new;
}

unsigned long IntMapHashFunction(long key, unsigned long mask) {
    return key & mask;
}

IntMap *IntMapNew(unsigned long capacity) {
    IntMap *map = malloc(sizeof(IntMap));
    map->capacity = capacity;
    map->mask = capacity - 1;
    map->size = 0;
    map->indexes = MapIndexNew(capacity);
    map->threashold = (unsigned long)(HT_LOAD * map->capacity);
    map->_free_value = NULL;
    map->entries = calloc(map->capacity, sizeof(IntMapNode *));
    return map;
}

void IntMapSetfreeValue(IntMap *map, void (*_free_value)(void *value)) {
    map->_free_value = _free_value;
}

IntMapNode *IntMapNodeNew(long key, void *value) {
    IntMapNode *n = malloc(sizeof(IntMapNode));
    n->key = key;
    n->value = value;
    return n;
}

static unsigned long
IntMapGetNextIdx(IntMap *map, long key,
                 int *_is_free) { // Finds the next avalible slot and marks it
                                  // in the bit vector as set
    unsigned long mask = map->mask;
    unsigned long idx = key & mask;
    unsigned long probe = 1;
    IntMapNode *cur;
    *_is_free = 0;
    while ((cur = map->entries[idx]) != NULL) {
        if (cur->key == key || cur->key == HT_DELETED) {
            *_is_free = 0;
            return idx;
        }
        idx = (idx + HT_PROBE_1 * probe + HT_PROBE_3 * probe * probe) & mask;
        probe++;
    }
    *_is_free = 1;
    return idx;
}

void IntMapRelease(IntMap *map) { // free the entire hashtable
    if (map) {
        void (*free_value)(void *value) = map->_free_value;
        for (long i = 0; i < map->capacity; ++i) {
            IntMapNode *n = map->entries[i];
            if (n) {
                if (free_value)
                    free_value(n->value);
                free(n);
            }
        }
        free(map->entries);
        free(map->indexes);
        free(map);
    }
}

int IntMapResize(IntMap *map) {
    // Resize the hashtable, will return false if OMM
    unsigned long new_capacity, old_capacity, new_mask;
    IntMapNode **new_entries, **old_entries;
    MapIndex *new_indexes;
    int is_free;

    old_entries = map->entries;
    old_capacity = map->capacity;
    new_capacity = map->capacity << 1;
    new_mask = new_capacity - 1;

    /* OOM */
    if ((new_indexes = MapIndexNew(map->indexes->capacity)) == NULL) {
        return 0;
    }

    /* OOM */
    if ((new_entries = calloc(new_capacity , sizeof(IntMapNode *))) == NULL) {
        free(new_indexes);
        return 0;
    }

    map->mask = new_mask;
    map->entries = new_entries;
    map->capacity = new_capacity;

    for (long i = 0; i < old_capacity; ++i) {
        IntMapNode *old = old_entries[i];
        if (old) {
            long key = old->key;
            if (key != HT_DELETED) {
                unsigned long idx = IntMapGetNextIdx(map, key, &is_free);
                new_entries[idx] = old;
                new_indexes->entries[new_indexes->len++] = idx;
            } else {
                free(old);
            }
        }
    }

    free(old_entries);
    free(map->indexes);
    map->indexes = new_indexes;
    map->threashold = (unsigned long)(map->capacity * HT_LOAD);
    return 1;
}

int IntMapSet(IntMap *map, long key, void *value) {
    int is_free;

    if (map->size >= map->threashold) {
        if (!IntMapResize(map)) {
            /* This means we have run out of memory */
            return 0;
        }
    }

    unsigned long idx = IntMapGetNextIdx(map, key, &is_free);
    if (is_free) {
        IntMapNode *n = IntMapNodeNew(key, value);
        map->entries[idx] = n;
        if (map->indexes->len + 1 >= map->indexes->capacity) {
            map->indexes = MapIndexReAlloc(map->indexes);
        }
        map->indexes->entries[map->indexes->len++] = idx;
        map->size++;
        return 1;
    } else {
        IntMapNode *n = map->entries[idx];
        n->key = key;
        n->value = value;
        return 1;
    }
}

int IntMapDelete(IntMap *map, long key) {
    unsigned long idx, mask, probe;
    IntMapNode **entries = map->entries;
    IntMapNode *cur;
    mask = map->mask;
    idx = IntMapHashFunction(key, mask);
    probe = 1;
    while ((cur = entries[idx])) {
        if (cur->key == key) {
            cur->key = HT_DELETED;
            map->indexes->entries[idx] = HT_DELETED;
            map->size--;
            return 1;
        }
        idx = (idx + HT_PROBE_1 * probe + HT_PROBE_3 * probe * probe) & mask;
        probe++;
    }
    return 0;
}

void *IntMapGet(IntMap *map, long key) {
    unsigned long idx, mask, probe;
    IntMapNode **entries = map->entries;
    IntMapNode *cur;

    mask = map->mask;
    probe = 1;
    idx = IntMapHashFunction(key, mask);
    while ((cur = entries[idx])) {
        if (cur->key == key) {
            return cur->value;
        }
        idx = (idx + HT_PROBE_1 * probe + HT_PROBE_3 * probe * probe) & mask;
        probe++;
    }
    return NULL;
}

int IntMapIter(IntMap *map, long *_idx, IntMapNode **_node) {
    long idx = *_idx;
    MapIndex *indexes = map->indexes;
    while (idx < indexes->len) {
        unsigned long index = indexes->entries[idx];
        if (index != HT_DELETED) {
            *_idx = idx + 1;
            *_node = map->entries[index];
            return 1;
        }
        idx++;
    }
    return 0;
}

int IntMapValueIter(IntMap *map, long *_idx, void **_value) {
    IntMapNode *node;
    if (IntMapIter(map, _idx, &node)) {
        *_value = node->value;
        return 1;
    }
    return 0;
}

int IntMapKeyIter(IntMap *map, long *_idx, long *_key) {
    IntMapNode *node;
    if (IntMapIter(map, _idx, &node)) {
        *_key = node->key;
        return 1;
    }
    return 0;
}

unsigned long StrMapHashFunction(char *key, long key_len, unsigned long mask) {
    unsigned long hash = 0;
    for (long i = 0; i < key_len; ++i) {
        hash = ((hash << 5) - hash) + key[i];
    }
    return hash & mask;
}

StrMap *StrMapNew(unsigned long capacity) {
    StrMap *map = malloc(sizeof(StrMap));
    map->capacity = capacity;
    map->mask = capacity - 1;
    map->size = 0;
    map->indexes = MapIndexNew(capacity);
    map->threashold = (unsigned long)(HT_LOAD * map->capacity);
    map->_free_value = NULL;
    map->_free_key = NULL;
    map->entries = calloc(map->capacity, sizeof(StrMapNode *));
    return map;
}

void StrMapSetfreeValue(StrMap *map, void (*_free_value)(void *value)) {
    map->_free_value = _free_value;
}

void StrMapSetfreeKey(StrMap *map, void (*_free_key)(void *key)) {
    map->_free_key = _free_key;
}

StrMapNode *StrMapNodeNew(char *key, long key_len, void *value) {
    StrMapNode *n = malloc(sizeof(StrMapNode));
    n->key = key;
    n->key_len = key_len;
    n->value = value;
    return n;
}

static unsigned long
StrMapGetNextIdx(StrMap *map, char *key, long key_len,
                 int *_is_free) { // Finds the next avalible slot and marks it
                                  // in the bit vector as set
    unsigned long mask = map->mask;
    unsigned long idx = StrMapHashFunction(key, key_len, mask);
    unsigned long probe = 1;
    StrMapNode *cur;
    *_is_free = 0;
    while ((cur = map->entries[idx]) != NULL) {
        if (cur->key == NULL) {
            *_is_free = 0;
            return idx;
        } else if (!strncmp(cur->key, key, cur->key_len)) {
            *_is_free = 0;
            return idx;
        }
        idx = (idx + HT_PROBE_1 * probe + HT_PROBE_3 * probe * probe) & mask;
        probe++;
    }
    *_is_free = 1;
    return idx;
}

void StrMapRelease(StrMap *map) { // free the entire hashtable
    if (map) {
        void (*free_value)(void *_val) = map->_free_value;
        void (*free_key)(void *_key) = map->_free_key;
        for (long i = 0; i < map->capacity; ++i) {
            StrMapNode *n = map->entries[i];
            if (n) {
                if (free_value)
                    free_value(n->value);
                if (free_key)
                    free_key(n->key);
                free(n);
            }
        }
        free(map->entries);
        free(map->indexes);
        free(map);
    }
}

 // Resize the hashtable, will return false if OMM
int StrMapResize(StrMap *map) {
    unsigned long new_capacity, old_capacity, new_mask;
    StrMapNode **new_entries, **old_entries;
    MapIndex *new_indexes;
    int is_free;

    old_entries = map->entries;
    old_capacity = map->capacity;
    new_capacity = map->capacity << 1;
    new_mask = new_capacity - 1;

    /* OOM */
    if ((new_indexes = MapIndexNew(map->indexes->capacity)) == NULL) {
        return 0;
    }

    /* OOM */
    if ((new_entries = calloc(new_capacity, sizeof(StrMapNode *))) == NULL) {
        free(new_indexes);
        return 0;
    }

    map->mask = new_mask;
    map->entries = new_entries;
    map->capacity = new_capacity;

    for (long i = 0; i < old_capacity; ++i) {
        StrMapNode *old = old_entries[i];
        if (old) {
            char *key = old->key;
            if (key != NULL) {
                unsigned long idx = StrMapGetNextIdx(map, key, old->key_len,
                                                     &is_free);
                new_entries[idx] = old;
                new_indexes->entries[new_indexes->len++] = idx;
            } else {
                free(old);
            }
        }
    }

    free(old_entries);
    free(map->indexes);
    map->indexes = new_indexes;
    map->threashold = (unsigned long)(map->capacity * HT_LOAD);
    return 1;
}

int StrMapSet(StrMap *map, char *key, void *value) {
    int is_free;

    if (map->size >= map->threashold) {
        if (!StrMapResize(map)) {
            /* This means we have run out of memory */
            return 0;
        }
    }

    long key_len = strlen(key);
    unsigned long idx = StrMapGetNextIdx(map, key, key_len, &is_free);

    if (is_free) {
        StrMapNode *n = StrMapNodeNew(key, key_len, value);
        map->entries[idx] = n;
        if (map->indexes->len + 1 >= map->indexes->capacity) {
            map->indexes = MapIndexReAlloc(map->indexes);
        }
        map->indexes->entries[map->indexes->len++] = idx;
        map->size++;
        return 1;
    } else {
        StrMapNode *n = map->entries[idx];
        n->key = key;
        n->key_len = key_len;
        n->value = value;
        return 1;
    }
}

int StrMapDelete(StrMap *map, char *key) {
    unsigned long idx, mask, probe;
    long len = strlen(key);
    StrMapNode **entries = map->entries;
    StrMapNode *cur;
    mask = map->mask;
    idx = StrMapHashFunction(key, len, mask);
    probe = 1;
    void (*free_value)(void *_val) = map->_free_value;
    void (*free_key)(void *_key) = map->_free_key;
    while ((cur = entries[idx])) {
        if (cur->key_len == len && !strncmp(cur->key, key, len)) {
            if (free_key)
                free_key(cur->key);
            if (free_value)
                free_value(cur->value);
            cur->value = cur->key = NULL;
            map->indexes->entries[idx] = HT_DELETED;
            map->size--;
            return 1;
        }
        idx = (idx + HT_PROBE_1 * probe + HT_PROBE_3 * probe * probe) & mask;
        probe++;
    }
    return 0;
}

void *StrMapGet(StrMap *map, char *key) {
    unsigned long idx, mask, probe;
    long len = strlen(key);
    StrMapNode **entries = map->entries;
    StrMapNode *cur;

    mask = map->mask;
    probe = 1;
    idx = StrMapHashFunction(key, len, mask);
    while ((cur = entries[idx])) {
        if (cur->key == NULL) {
            return NULL;
        }
        if (cur->key_len == len && !strncmp(cur->key, key, len)) {
            return cur->value;
        }
        idx = (idx + HT_PROBE_1 * probe + HT_PROBE_3 * probe * probe) & mask;
        probe++;
    }
    return NULL;
}

int StrMapIter(StrMap *map, long *_idx, StrMapNode **_node) {
    long idx = *_idx;
    MapIndex *indexes = map->indexes;
    while (idx < indexes->len) {
        long index = indexes->entries[idx];
        if (index != HT_DELETED) {
            *_idx = idx + 1;
            *_node = map->entries[index];
            return 1;
        }
        idx++;
    }
    return 0;
}

int StrMapValueIter(StrMap *map, long *_idx, void **_value) {
    StrMapNode *node;
    if (StrMapIter(map, _idx, &node)) {
        *_value = node->value;
        return 1;
    }
    return 0;
}

int StrMapKeyIter(StrMap *map, long *_idx, char **_key) {
    StrMapNode *node;
    if (StrMapIter(map, _idx, &node)) {
        *_key = node->key;
        return 1;
    }
    return 0;
}
