#ifndef BITMAP_H
#define BITMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "common/assert.h"

/*
 * Fixed-storage bitmap.
 *
 * Usage:
 *   1. Use bitmap_storage() to declare storage arrays.
 *   2. Initialize runtime objects with bitmap_init(), or use
 *      bitmap_static_init() for static-storage bitmap objects.
 *   3. Use bitmap_set()/bitmap_clear()/bitmap_test() for direct bit access.
 *   4. Use bitmap_find_first_set() when the lowest-numbered active bit is
 *      needed, such as priority maps or flag scanning.
 *   5. Use bitmap_find_first_zero() for simple fixed-ID/resource allocation.
 *
 * Design notes:
 *   - The bitmap never allocates memory. Storage lifetime is owned by the caller.
 *   - Parameter validity is not checked by default; callers must satisfy API
 *     preconditions.
 *   - Define BITMAP_DIAGNOSTIC_ENABLE to 1 when debugging misuse; it enables
 *     ASSERT-based parameter and bounds checks.
 *   - Bits are indexed from 0. The first bit is the least significant bit of
 *     words[0].
 *   - Tail bits beyond bit_count in the final word are ignored by query APIs.
 *
 * Concurrency:
 *   This module does not provide any concurrency control. Callers in concurrent
 *   environments should protect the structure with appropriate synchronization.
 *
 * API quick reference:
 *   - Storage and initialization:
 *       bitmap_storage()
 *       bitmap_static_init()
 *       bitmap_init()
 *
 *   - Whole-bitmap operations:
 *       bitmap_clear_all()
 *       bitmap_set_all()
 *
 *   - Single-bit operations:
 *       bitmap_set()
 *       bitmap_clear()
 *       bitmap_test()
 *       bitmap_test_and_set()
 *       bitmap_test_and_clear()
 *
 *   - State queries:
 *       bitmap_bit_count()
 *       bitmap_is_empty()
 *       bitmap_is_full()
 *       bitmap_count()              O(n), prefer caller-maintained counters on hot paths
 *
 *   - Search:
 *       bitmap_find_first_set()     find the lowest-numbered set bit
 *       bitmap_find_first_zero()    find the lowest-numbered clear bit
 *
 * Examples:
 *   bitmap_storage(flag_storage, 32);
 *   bitmap_t flags = bitmap_static_init(flag_storage, 32);
 *
 *   bitmap_clear_all(&flags);
 *   bitmap_set(&flags, 3);
 *
 *   uint32_t bit;
 *   if (bitmap_find_first_set(&flags, &bit)) {
 *       // bit == 3
 *   }
 */

typedef uint32_t bitmap_word_t;

enum {
    BITMAP_WORD_BITS_ = 32U,
    BITMAP_WORD_SHIFT_ = 5U,
    BITMAP_WORD_MASK_ = BITMAP_WORD_BITS_ - 1U
};

typedef struct bitmap {
    bitmap_word_t *words;
    uint32_t bit_count;
} bitmap_t;

#ifndef BITMAP_DIAGNOSTIC_ENABLE
#define BITMAP_DIAGNOSTIC_ENABLE 0
#endif

#if BITMAP_DIAGNOSTIC_ENABLE
#define BITMAP_ASSERT(cond) ASSERT(cond)
#else
#define BITMAP_ASSERT(cond) ((void)sizeof(cond))
#endif

/*
 * Internal helper: return how many bitmap_word_t elements are needed to store
 * bit_count bits.
 */
#define bitmap_word_count_(bit_count) \
    (((bit_count) / BITMAP_WORD_BITS_) + (((bit_count) & BITMAP_WORD_MASK_) != 0U))

/*
 * Declare a storage array for bit_count bits.
 * for example, bitmap_storage(flag_words, 32) declares an array of one
 * bitmap_word_t element, which can store 32 bits.
 *
 * Example:
 *   static bitmap_storage(flag_words, 32);
 */
#define bitmap_storage(name, bit_count) \
    bitmap_word_t name[bitmap_word_count_(bit_count)]

/*
 * Static initializer.
 *
 * This initializes the bitmap object, not the storage contents. Use zeroed
 * storage or call bitmap_clear_all() before first use when an empty bitmap is
 * required.
 *
 * Example:
 *   static bitmap_storage(words, 32);
 *   static bitmap_t flags = bitmap_static_init(words, 32);
 */
#define bitmap_static_init(words_, bit_count_) { (words_), (uint32_t)(bit_count_) }

static inline uint32_t bitmap_word_index_(uint32_t bit)
{
    return bit >> BITMAP_WORD_SHIFT_;
}

static inline uint32_t bitmap_bit_offset_(uint32_t bit)
{
    return bit & BITMAP_WORD_MASK_;
}

static inline bitmap_word_t bitmap_bit_mask_(uint32_t bit)
{
    return (bitmap_word_t)1U << bitmap_bit_offset_(bit);
}

static inline bitmap_word_t bitmap_tail_mask_(uint32_t bit_count)
{
    uint32_t used_bits = bit_count & BITMAP_WORD_MASK_;

    if (used_bits == 0U) {
        return UINT32_MAX;
    }

    return ((bitmap_word_t)1U << used_bits) - 1U;
}

static inline uint32_t bitmap_ctz_(bitmap_word_t word)
{
    BITMAP_ASSERT(word != 0U);
#if defined(__GNUC__) || defined(__clang__)
    return (uint32_t)__builtin_ctz((unsigned int)word);
#else
    uint32_t bit = 0;

    while ((word & 1U) == 0U) {
        bit++;
        word >>= 1;
    }

    return bit;
#endif
}

static inline uint32_t bitmap_popcount_(bitmap_word_t word)
{
#if defined(__GNUC__) || defined(__clang__)
    return (uint32_t)__builtin_popcount((unsigned int)word);
#else
    uint32_t count = 0;

    while (word != 0U) {
        word &= word - 1U;
        count++;
    }

    return count;
#endif
}

/*
 * Initialize bitmap and clear all bits.
 *
 * words must point to storage declared by bitmap_storage() for bit_count bits
 * when bit_count is nonzero.
 */
static inline void bitmap_init(bitmap_t *bm,
                               bitmap_word_t *words,
                               uint32_t bit_count);

/* Clear every valid bit. */
static inline void bitmap_clear_all(bitmap_t *bm)
{
    uint32_t word_count;
    uint32_t i;

    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT((bm->words != NULL) || (bm->bit_count == 0U));

    word_count = bitmap_word_count_(bm->bit_count);
    for (i = 0; i < word_count; i++) {
        bm->words[i] = 0U;
    }
}

/* Set every valid bit. Tail bits beyond bit_count remain clear. */
static inline void bitmap_set_all(bitmap_t *bm)
{
    uint32_t word_count;
    uint32_t i;

    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT((bm->words != NULL) || (bm->bit_count == 0U));

    word_count = bitmap_word_count_(bm->bit_count);
    for (i = 0; i < word_count; i++) {
        bm->words[i] = UINT32_MAX;
    }

    if (word_count != 0U) {
        bm->words[word_count - 1U] &= bitmap_tail_mask_(bm->bit_count);
    }
}

static inline void bitmap_init(bitmap_t *bm,
                               bitmap_word_t *words,
                               uint32_t bit_count)
{
    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT((words != NULL) || (bit_count == 0U));

    bm->words = words;
    bm->bit_count = bit_count;
    bitmap_clear_all(bm);
}

/* Return the number of valid bits managed by bm. */
static inline uint32_t bitmap_bit_count(const bitmap_t *bm)
{
    BITMAP_ASSERT(bm != NULL);
    return bm->bit_count;
}

/*
 * Set bit.
 *
 * bit must be less than bitmap_bit_count(bm).
 */
static inline void bitmap_set(bitmap_t *bm, uint32_t bit)
{
    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT(bm->words != NULL);
    BITMAP_ASSERT(bit < bm->bit_count);

    bm->words[bitmap_word_index_(bit)] |= bitmap_bit_mask_(bit);
}

/*
 * Clear bit.
 *
 * bit must be less than bitmap_bit_count(bm).
 */
static inline void bitmap_clear(bitmap_t *bm, uint32_t bit)
{
    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT(bm->words != NULL);
    BITMAP_ASSERT(bit < bm->bit_count);

    bm->words[bitmap_word_index_(bit)] &= ~bitmap_bit_mask_(bit);
}

/*
 * Return true when bit is set.
 *
 * bit must be less than bitmap_bit_count(bm).
 */
static inline bool bitmap_test(const bitmap_t *bm, uint32_t bit)
{
    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT(bm->words != NULL);
    BITMAP_ASSERT(bit < bm->bit_count);

    return (bm->words[bitmap_word_index_(bit)] & bitmap_bit_mask_(bit)) != 0U;
}

/*
 * Set bit and return its previous state.
 *
 * Return true when bit was already set.
 */
static inline bool bitmap_test_and_set(bitmap_t *bm, uint32_t bit)
{
    bitmap_word_t mask;
    bitmap_word_t *word;
    bool was_set;

    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT(bm->words != NULL);
    BITMAP_ASSERT(bit < bm->bit_count);

    word = &bm->words[bitmap_word_index_(bit)];
    mask = bitmap_bit_mask_(bit);
    was_set = ((*word & mask) != 0U);
    *word |= mask;
    return was_set;
}

/*
 * Clear bit and return its previous state.
 *
 * Return true when bit was set before it was cleared.
 */
static inline bool bitmap_test_and_clear(bitmap_t *bm, uint32_t bit)
{
    bitmap_word_t mask;
    bitmap_word_t *word;
    bool was_set;

    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT(bm->words != NULL);
    BITMAP_ASSERT(bit < bm->bit_count);

    word = &bm->words[bitmap_word_index_(bit)];
    mask = bitmap_bit_mask_(bit);
    was_set = ((*word & mask) != 0U);
    *word &= ~mask;
    return was_set;
}

/* Return true when no valid bit is set. */
static inline bool bitmap_is_empty(const bitmap_t *bm)
{
    uint32_t word_count;
    uint32_t i;

    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT((bm->words != NULL) || (bm->bit_count == 0U));

    word_count = bitmap_word_count_(bm->bit_count);
    for (i = 0; i < word_count; i++) {
        bitmap_word_t word = bm->words[i];

        if (i == (word_count - 1U)) {
            word &= bitmap_tail_mask_(bm->bit_count);
        }

        if (word != 0U) {
            return false;
        }
    }

    return true;
}

/* Return true when every valid bit is set. */
static inline bool bitmap_is_full(const bitmap_t *bm)
{
    uint32_t word_count;
    uint32_t i;

    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT((bm->words != NULL) || (bm->bit_count == 0U));

    word_count = bitmap_word_count_(bm->bit_count);
    for (i = 0; i < word_count; i++) {
        bitmap_word_t mask = UINT32_MAX;

        if (i == (word_count - 1U)) {
            mask = bitmap_tail_mask_(bm->bit_count);
        }

        if ((bm->words[i] & mask) != mask) {
            return false;
        }
    }

    return true;
}

/*
 * Count set bits.
 *
 * This is an O(n) traversal over bitmap words. It is useful for diagnostics,
 * low-frequency inspection code, or resource accounting. For hot paths,
 * prefer maintaining a separate counter in the owner object.
 */
static inline uint32_t bitmap_count(const bitmap_t *bm)
{
    uint32_t word_count;
    uint32_t i;
    uint32_t count = 0;

    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT((bm->words != NULL) || (bm->bit_count == 0U));

    word_count = bitmap_word_count_(bm->bit_count);
    for (i = 0; i < word_count; i++) {
        bitmap_word_t word = bm->words[i];

        if (i == (word_count - 1U)) {
            word &= bitmap_tail_mask_(bm->bit_count);
        }

        count += bitmap_popcount_(word);
    }

    return count;
}

/*
 * Find the lowest-numbered set bit.
 *
 * Store the bit index in *bit and return true when found. Return false when
 * the bitmap is empty.
 */
static inline bool bitmap_find_first_set(const bitmap_t *bm, uint32_t *bit)
{
    uint32_t word_count;
    uint32_t i;

    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT(bit != NULL);
    BITMAP_ASSERT((bm->words != NULL) || (bm->bit_count == 0U));

    word_count = bitmap_word_count_(bm->bit_count);
    for (i = 0; i < word_count; i++) {
        bitmap_word_t word = bm->words[i];

        if (i == (word_count - 1U)) {
            word &= bitmap_tail_mask_(bm->bit_count);
        }

        if (word != 0U) {
            *bit = (i * BITMAP_WORD_BITS_) + bitmap_ctz_(word);
            return true;
        }
    }

    return false;
}

/*
 * Find the lowest-numbered clear bit.
 *
 * Store the bit index in *bit and return true when found. Return false when
 * every valid bit is set.
 */
static inline bool bitmap_find_first_zero(const bitmap_t *bm, uint32_t *bit)
{
    uint32_t word_count;
    uint32_t i;

    BITMAP_ASSERT(bm != NULL);
    BITMAP_ASSERT(bit != NULL);
    BITMAP_ASSERT((bm->words != NULL) || (bm->bit_count == 0U));

    word_count = bitmap_word_count_(bm->bit_count);
    for (i = 0; i < word_count; i++) {
        bitmap_word_t word = ~bm->words[i];

        if (i == (word_count - 1U)) {
            word &= bitmap_tail_mask_(bm->bit_count);
        }

        if (word != 0U) {
            *bit = (i * BITMAP_WORD_BITS_) + bitmap_ctz_(word);
            return true;
        }
    }

    return false;
}

#ifdef __cplusplus
}
#endif

#endif /* BITMAP_H */
