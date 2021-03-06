/**
 * @file hash_table.h
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief libyang hash table
 *
 * Copyright (c) 2015 - 2018 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef LY_HASH_TABLE_H_
#define LY_HASH_TABLE_H_

#include <stdint.h>
#include <pthread.h>

#include "common.h"
#include "dict.h"

/**
 * size of the dictionary for each context
 */
#define DICT_SIZE 1024

/**
 * record of the dictionary
 * TODO: save the next pointer by different collision strategy, will need to
 * make dictionary size dynamic
 */
struct dict_rec {
    struct dict_rec *next;
    char *value;
    uint32_t refcount:22;
    uint32_t len:10;
#define DICT_REC_MAXCOUNT 0x003fffff
#define DICT_REC_MAXLEN   0x000003ff
};

/**
 * dictionary to store repeating strings
 * TODO: make it variable size
 */
struct dict_table {
    struct dict_rec recs[DICT_SIZE];
    int hash_mask;
    uint32_t used;
    pthread_mutex_t lock;
};

/**
 * @brief Initiate content (non-zero values) of the dictionary
 *
 * @param[in] dict Dictionary table to initiate
 */
void lydict_init(struct dict_table *dict);

/**
 * @brief Cleanup the dictionary content
 *
 * @param[in] dict Dictionary table to cleanup
 */
void lydict_clean(struct dict_table *dict);

/**
 * @brief Compute hash from (several) string(s).
 *
 * Usage:
 * - init hash to 0
 * - repeatedly call dict_hash_multi(), provide hash from the last call
 * - call dict_hash_multi() with key_part = NULL to finish the hash
 */
uint32_t dict_hash_multi(uint32_t hash, const char *key_part, size_t len);

/**
 * @brief Callback for checking hash table values equivalence.
 *
 * @param[in] val1_p Pointer to the first value.
 * @param[in] val2_p Pointer to the second value.
 * @param[in] mod Whether the operation modifies the hash table (insert or remove) or not (find).
 * @param[in] cb_data User callback data.
 * @return 0 on non-equal, non-zero on equal.
 */
typedef int (*values_equal_cb)(void *val1_p, void *val2_p, int mod, void *cb_data);

/** when the table is at least this much percent full, it is enlarged (double the size) */
#define LYHT_ENLARGE_PERCENTAGE 75

/** only once the table is this much percent full, enable shrinking */
#define LYHT_FIRST_SHRINK_PERCENTAGE 50

/** when the table is less than this much percent full, it is shrunk (half the size) */
#define LYHT_SHRINK_PERCENTAGE 25

/** never shrink beyond this size */
#define LYHT_MIN_SIZE 8

/**
 * @brief Generic hash table record.
 */
struct ht_rec {
    uint32_t hash;        /* hash of the value */
    int32_t hits;         /* collision/overflow value count - 1 (a filled entry has 1 hit,
                           * special value -1 means a deleted record) */
    unsigned char val[1]; /* arbitrary-size value */
} _PACKED;

/**
 * @brief (Very) generic hash table.
 *
 * Hash table with open addressing collision resolution and
 * linear probing of interval 1 (next free record is used).
 * Removal is lazy (removed records are only marked), but
 * if possible, they are fully emptied.
 */
struct hash_table {
    uint32_t used;        /* number of values stored in the hash table (filled records) */
    uint32_t size;        /* always holds 2^x == size (is power of 2), actually number of records allocated */
    values_equal_cb val_equal; /* callback for testing value equivalence */
    void *cb_data;        /* user data callback arbitrary value */
    uint16_t resize;      /* 0 - resizing is disabled, *
                           * 1 - enlarging is enabled, *
                           * 2 - both shrinking and enlarging is enabled */
    uint16_t rec_size;    /* real size (in bytes) of one record for accessing recs array */
    unsigned char *recs;  /* pointer to the hash table itself (array of struct ht_rec) */
};

/**
 * @brief Create new hash table.
 *
 * @param[in] size Starting size of the hash table (capacity of values), must be power of 2.
 * @param[in] val_size Size in bytes of value (the stored hashed item).
 * @param[in] val_equal Callback for checking value equivalence.
 * @param[in] cb_data User data always passed to \p val_equal.
 * @param[in] resize Whether to resize the table on too few/too many records taken.
 * @return Empty hash table, NULL on error.
 */
struct hash_table *lyht_new(uint32_t size, uint16_t val_size, values_equal_cb val_equal, void *cb_data, int resize);

/**
 * @brief Set hash table value equal callback.
 *
 * @param[in] ht Hash table to modify.
 * @param[in] new_val_equal New callback for checking value equivalence.
 * @return Previous callback for checking value equivalence.
 */
values_equal_cb lyht_set_cb(struct hash_table *ht, values_equal_cb new_val_equal);

/**
 * @brief Set hash table value equal callback user data.
 *
 * @param[in] ht Hash table to modify.
 * @param[in] new_cb_data New data for values callback.
 * @return Previous data for values callback.
 */
void *lyht_set_cb_data(struct hash_table *ht, void *new_cb_data);

/**
 * @brief Make a duplicate of an existing hash table.
 *
 * @param[in] orig Original hash table to duplicate.
 * @return Duplicated hash table \p orig, NULL on error.
 */
struct hash_table *lyht_dup(const struct hash_table *orig);

/**
 * @brief Free a hash table.
 *
 * @param[in] ht Hash table to be freed.
 */
void lyht_free(struct hash_table *ht);

/**
 * @brief Find a value in a hash table.
 *
 * @param[in] ht Hash table to search in.
 * @param[in] val_p Pointer to the value to find.
 * @param[in] hash Hash of the stored value.
 * @param[out] match_p pointer to the matching value, optional.
 * @return 0 on success, 1 on not found.
 */
int lyht_find(struct hash_table *ht, void *val_p, uint32_t hash, void **match_p);

/**
 * @brief Find another equal value in the hash table.
 *
 * @param[in] ht Hash table to search in.
 * @param[in] val_p Pointer to the previously found value in \p ht.
 * @param[in] hash Hash of the previously found value.
 * @param[out] match_p pointer to the matching value, optional.
 * @return 0 on success, 1 on not found.
 */
int lyht_find_next(struct hash_table *ht, void *val_p, uint32_t hash, void **match_p);

/**
 * @brief Insert a value into a hash table.
 *
 * @param[in] ht Hash table to insert into.
 * @param[in] val_p Pointer to the value to insert. Be careful, if the values stored in the hash table
 * are pointers, \p val_p must be a pointer to a pointer.
 * @param[in] hash Hash of the stored value.
 * @return 0 on success, 1 if already inserted, -1 on error.
 */
int lyht_insert(struct hash_table *ht, void *val_p, uint32_t hash);

/**
 * @brief Remove a value from a hash table.
 *
 * @param[in] ht Hash table to remove from.
 * @param[in] value_p Pointer to value to be removed. Be careful, if the values stored in the hash table
 * are pointers, \p value_p must be a pointer to a pointer.
 * @param[in] hash Hash of the stored value.
 * @return 0 on success, 1 if value was not found, -1 on error.
 */
int lyht_remove(struct hash_table *ht, void *val_p, uint32_t hash);

#endif /* LY_HASH_TABLE_H_ */
