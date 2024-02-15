#ifndef MAP_H
#define MAP_H 

#define HT_LOAD    0.60
#define HT_DELETED LONG_MAX
#define HT_PROBE_1 1
#define HT_PROBE_3 3

typedef struct MapIndex {
    long capacity;
    long len;
    long *entries;
} MapIndex;

typedef struct IntMapNode {
    long key;
    void *value;
} IntMapNode;

typedef struct StrMapNode {
    char *key;
    long key_len;
    void *value;
} StrMapNode;

typedef struct IntMap {
    unsigned long size;     /* How many entries are in the hashtable */
    unsigned long capacity; /* How much capacity we have in the entries array */
    unsigned long
            mask; /* Used for hashing, as the capacity is always a power of 2
                   * we can use fast modulo of `<int> & capacity-1`. */
    MapIndex *indexes; /* Where all of the values are in the entries array, in
                        * insertion order. Means we can iterate over the
                        * HashTable quickly at the cost of memory */
    unsigned long threashold; /* rebuild threashold */
    void (*_free_value)(
            void *value); /* User defined callback for freeing values */
    IntMapNode **entries; /* All of the entries, XXX: could this be IntMapNode
                           *entries? */
} IntMap;

typedef struct StrMap {
    unsigned long size;     /* How many entries are in the hashtable */
    unsigned long capacity; /* How much capacity we have in the entries array */
    unsigned long
            mask; /* Used for hashing, as the capacity is always a power of 2
                   * we can use fast modulo of `<int> & capacity-1`. */
    MapIndex *indexes; /* Where all of the values are in the entries array, in
                        * insertion order. Means we can iterate over the
                        * HashTable quickly at the cost of memory */
    unsigned long threashold; /* rebuild threashold */
    void (*_free_value)(
            void *_value); /* User defined callback for freeing values */
    void (*_free_key)(void *_key); /* User defined callback for freeing keys */
    StrMapNode **entries; /* All of the entries, XXX: could this be IntMapNode
                           *entries? */
} StrMap;

int IntMapSet(IntMap *map, long key, void *value);
void *IntMapGet(IntMap *map, long key);
IntMap *IntMapNew(unsigned long capacity);
void IntMapSetfreeValue(IntMap *map, void (*_free_value)(void *value));
void IntMapRelease(IntMap *map);
int IntMapResize(IntMap *map);
int IntMapIter(IntMap *map, long *_idx, IntMapNode **_node);
int IntMapValueIter(IntMap *map, long *_idx, void **_value);
int IntMapKeyIter(IntMap *map, long *_idx, long *_key);

int StrMapSet(StrMap *map, char *key, void *value);
void *StrMapGet(StrMap *map, char *key);
StrMap *StrMapNew(unsigned long capacity);
void StrMapSetfreeValue(StrMap *map, void (*_free_value)(void *value));
void StrMapSetfreeKey(StrMap *map, void (*_free_key)(void *key));
void StrMapRelease(StrMap *map);
int StrMapResize(StrMap *map);
int StrMapIter(StrMap *map, long *_idx, StrMapNode **_node);
int StrMapValueIter(StrMap *map, long *_idx, void **_value);
int StrMapKeyIter(StrMap *map, long *_idx, char **_key);

#endif
