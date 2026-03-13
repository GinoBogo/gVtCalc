/* ************************************************************************** */
/*
    @file
        gb_utils.c

    @date
        October, 2023

    @author
        Gino Francesco Bogo (ᛊᛟᚱᚱᛖ ᛗᛖᚨ ᛁᛊᛏᚨᛗᛁ ᚨcᚢᚱᛉᚢ)

    @license
        MIT
 */
/* ************************************************************************** */

#include "gb_utils.h"

#include <errno.h>  // errno, ERANGE
#include <stdint.h> // SIZE_MAX, uint32_t, uintptr_t
#include <stdio.h>  // size_t, snprintf
#include <stdlib.h> // NULL, malloc, strtoul

// Compile-time checks for portability
#if !defined(UINT32_MAX) || (UINT32_MAX != 0xFFFFFFFFU)
#error "This code requires a proper 32-bit unsigned integer type"
#endif

#if !defined(UINTPTR_MAX) || (UINTPTR_MAX < 0xFFFFFFFFU)
#error "This code requires at least 32-bit pointers"
#endif

#if (GB_WORD_BITS == 64) && (!defined(UINT64_MAX) || (UINT64_MAX != 0xFFFFFFFFFFFFFFFFULL))
#error "GB_WORD_BITS=64 requires a proper 64-bit unsigned integer type"
#endif

// Fixed 32-bit constant — used only in the 32-bit portability verify below
#define BYTES_PER_UINT32 4

// Word-width-derived constants — all hot-path code uses these
#define ALIGNMENT_MASK (sizeof(gb_word_t) - 1)
#define BYTES_PER_WORD sizeof(gb_word_t)

// Compile-time verification (C99 compatible)
typedef char verify_uint32_size[BYTES_PER_UINT32 == sizeof(uint32_t) ? 1 : -1];
typedef char verify_word_size[BYTES_PER_WORD == (GB_WORD_BITS / 8) ? 1 : -1];

/*
         HIGHER ADDRESS
    ┌──────────────────────┐
    │                      │
    │ Unmapped or Reserved │  Command-line argument & Environment Variables
    │                      │
    └──────────────────────┘ ....................
    ┌──────────────────────┐ ....................
    │    Stack Segment     │  Stack Frame
    │          ↓           │
    │                      │
    │                      │
    │                      │
    │          ↑           │
    │     Heap Segment     │  Dynamic Memory
    └──────────────────────┘ ....................
    ┌──────────────────────┐ ....................
    │  Uninitialized Data  │
    └──────────────────────┘  Data Segment
    ┌──────────────────────┐
    │   Initialized Data   │
    └──────────────────────┘ ....................
    ┌──────────────────────┐ ....................
    │                      │
    │     Text Segment     │  Executable Code
    │                      │
    └──────────────────────┘
         LOWER ADDRESS
 */

// *****************************************************************************
// *****************************************************************************
// Local Functions
// *****************************************************************************
// *****************************************************************************

// --- Endianness detection ----------------------------------------------------

// If GB_BIG_ENDIAN is not provided by the build system or platform header,
// detect endianness once via a union probe and store the result in a
// file-scope static const. Any optimiser with constant propagation will
// eliminate the branch entirely; the function call form below would
// re-evaluate on every expansion of the macro in hot-path inlined code.
#if !defined(GB_BIG_ENDIAN)
static inline int _detect_big_endian(void) {
    union {
        uint32_t      u;
        unsigned char b[4];
    } probe = {0x01000000UL};

    return probe.b[0] == 1;
}

static const int GB_BIG_ENDIAN = _detect_big_endian();
#endif

// --- Helpers for gb_memmove --------------------------------------------------

/**
 * @brief Copies memory backwards to handle overlapping regions where dst > src.
 *
 * Algorithm: peel enough trailing bytes to align the write end of dst to a
 * 4-byte boundary, then copy word-wide chunks in reverse order with an
 * 8-unrolled loop (each iteration decrements both pointers before writing),
 * then handle any remaining head bytes individually.
 *
 * @param[out] dst Pointer to the destination memory area.
 * @param[in]  src Pointer to the source memory area.
 * @param[in]  len Number of bytes to copy.
 */
static void _memcpy_backward(char        *dst, //
                             const char  *src, //
                             const size_t len) {
    if (len < 8) {
        for (size_t i = len; i > 0; i--) {
            dst[i - 1] = src[i - 1];
        }
        return;
    }

    size_t i = len;

    // Peel trailing bytes to align (dst + i) to a 4-byte boundary
    size_t tail = (uintptr_t)(dst + len) & ALIGNMENT_MASK;
    if (tail > i) {
        tail = i;
    }
    for (; tail > 0; tail--, i--) {
        dst[i - 1] = src[i - 1];
    }

    // Copy BYTES_PER_WORD bytes at a time going backwards;
    // pointers are decremented before each write
    size_t      len_w       = i / BYTES_PER_WORD;
    char       *dst_aligned = dst + i;
    const char *src_aligned = src + i;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"

#pragma GCC unroll 8
    for (size_t j = 0; j < len_w; j++) {
        dst_aligned -= BYTES_PER_WORD;
        src_aligned -= BYTES_PER_WORD;
        *(gb_word_t *)dst_aligned = *(const gb_word_t *)src_aligned;
    }

#pragma GCC diagnostic pop

    // Handle remaining head bytes
    i -= len_w * BYTES_PER_WORD;
    for (; i > 0; i--) {
        dst[i - 1] = src[i - 1];
    }
}

// --- Helpers for gb_strrchr --------------------------------------------------

/**
 * @brief Scans the unaligned head bytes of a string for the last occurrence
 * of a character, advancing the pointer until 4-byte alignment is reached.
 *
 * @param[in,out] cp             Current byte pointer (advanced in place).
 * @param[in]     c              Character to find.
 * @param[in,out] last_occurrence Updated whenever a match is found.
 * @return true if the null terminator was reached, false otherwise.
 */
static inline bool _process_unaligned_bytes(char    **cp, //
                                            const int c,  //
                                            char    **last_occurrence) {
    while ((uintptr_t)*cp & (sizeof(gb_word_t) - 1)) {
        if (**cp == '\0') {
            return true;
        }
        if (**cp == (char)c) {
            *last_occurrence = *cp;
        }
        ++(*cp);
    }
    return false;
}

/**
 * @brief Scans the bytes of a word that is known to contain a null terminator.
 *
 * Fully unrolled — no loop counter, no per-iteration pointer addition, no
 * repeated cast. Bytes are checked in memory order; last_occurrence is updated
 * for every match encountered before the null terminator.
 *
 * The 64-bit path checks 8 bytes; the 32-bit path checks 4.
 *
 * @param[in]     wp              Const word pointer (must be word-aligned).
 * @param[in]     c               Character to find.
 * @param[in,out] last_occurrence Updated whenever a match is found.
 * @return true when the null terminator byte is reached.
 */
static inline bool _process_word_with_zero(const gb_word_t *wp, //
                                           const int        c,  //
                                           char           **last_occurrence) {
    const char  cb   = (char)c;
    const char *base = (const char *)wp;

    // clang-format off
    if (base[0] == '\0') { return true; } if (base[0] == cb) { *last_occurrence = (char *)base;     }
    if (base[1] == '\0') { return true; } if (base[1] == cb) { *last_occurrence = (char *)base + 1; }
    if (base[2] == '\0') { return true; } if (base[2] == cb) { *last_occurrence = (char *)base + 2; }
    if (base[3] == '\0') { return true; } if (base[3] == cb) { *last_occurrence = (char *)base + 3; }
#if GB_WORD_BITS == 64
    if (base[4] == '\0') { return true; } if (base[4] == cb) { *last_occurrence = (char *)base + 4; }
    if (base[5] == '\0') { return true; } if (base[5] == cb) { *last_occurrence = (char *)base + 5; }
    if (base[6] == '\0') { return true; } if (base[6] == cb) { *last_occurrence = (char *)base + 6; }
    if (base[7] == '\0') { return true; } if (base[7] == cb) { *last_occurrence = (char *)base + 7; }
#endif
    // clang-format on
    return false;
}

/**
 * @brief Scans all bytes of a word for the target character.
 *
 * Called only for words that contain no null terminator, so every byte is
 * a valid string character and can be tested unconditionally. Fully unrolled
 * — no loop counter, no per-iteration pointer addition, no repeated cast.
 *
 * The 64-bit path checks 8 bytes; the 32-bit path checks 4.
 *
 * @param[in]     wp              Const word pointer (must be word-aligned).
 * @param[in]     c               Character to find.
 * @param[in,out] last_occurrence Updated whenever a match is found.
 */
static inline void _search_char_in_word(const gb_word_t *wp, //
                                        const int        c,  //
                                        char           **last_occurrence) {
    const char  cb   = (char)c;
    const char *base = (const char *)wp;

    // clang-format off
    if (base[0] == cb) { *last_occurrence = (char *)base;     }
    if (base[1] == cb) { *last_occurrence = (char *)base + 1; }
    if (base[2] == cb) { *last_occurrence = (char *)base + 2; }
    if (base[3] == cb) { *last_occurrence = (char *)base + 3; }
#if GB_WORD_BITS == 64
    if (base[4] == cb) { *last_occurrence = (char *)base + 4; }
    if (base[5] == cb) { *last_occurrence = (char *)base + 5; }
    if (base[6] == cb) { *last_occurrence = (char *)base + 6; }
    if (base[7] == cb) { *last_occurrence = (char *)base + 7; }
#endif
    // clang-format on
}

// --- Helpers for gb_strchr ---------------------------------------------------

/**
 * @brief Resolves the first relevant byte in a word known to contain a zero.
 *
 * Scans bytes in memory order. A null terminator encountered before a match
 * means the character is not present, so NULL is returned. A match
 * encountered before a null returns a pointer to that byte.
 * The 64-bit path checks bytes 0–7; the 32-bit path checks bytes 0–3.
 *
 * @param[in] cp Byte pointer to the start of the word (must be word-aligned).
 * @param[in] c  The target character (already cast to char).
 * @return Pointer to the first match, or NULL if a null terminator came first.
 */
static inline char *_strchr_in_zero_word(const char *cp, // NOLINT(readability-function-cognitive-complexity)
                                         const char  c) {
    if (cp[0] == '\0' || cp[0] == c) {
        return (cp[0] == c) ? (char *)cp : NULL;
    }
    if (cp[1] == '\0' || cp[1] == c) {
        return (cp[1] == c) ? (char *)cp + 1 : NULL;
    }
    if (cp[2] == '\0' || cp[2] == c) {
        return (cp[2] == c) ? (char *)cp + 2 : NULL;
    }
#if GB_WORD_BITS == 64
    if (cp[3] == '\0' || cp[3] == c) {
        return (cp[3] == c) ? (char *)cp + 3 : NULL;
    }
    if (cp[4] == '\0' || cp[4] == c) {
        return (cp[4] == c) ? (char *)cp + 4 : NULL;
    }
    if (cp[5] == '\0' || cp[5] == c) {
        return (cp[5] == c) ? (char *)cp + 5 : NULL;
    }
    if (cp[6] == '\0' || cp[6] == c) {
        return (cp[6] == c) ? (char *)cp + 6 : NULL;
    }
    return (cp[7] == c) ? (char *)cp + 7 : NULL;
#else
    return (cp[3] == c) ? (char *)cp + 3 : NULL;
#endif
}

/**
 * @brief Resolves the first matching byte in a word known to contain a match.
 *
 * The caller has already confirmed via GB_HAS_ZERO(w ^ cmask) that at least
 * one byte equals c and that the word contains no null terminator, so a
 * simple sequential scan is sufficient.
 * The 64-bit path checks bytes 0–7; the 32-bit path checks bytes 0–3.
 *
 * @param[in] cp Byte pointer to the start of the word (must be word-aligned).
 * @param[in] c  The target character (already cast to char).
 * @return Pointer to the first byte that equals c.
 */
static inline char *_strchr_in_match_word(const char *cp, //
                                          const char  c) {
    if (cp[0] == c) {
        return (char *)cp;
    }
    if (cp[1] == c) {
        return (char *)cp + 1;
    }
    if (cp[2] == c) {
        return (char *)cp + 2;
    }
#if GB_WORD_BITS == 64
    if (cp[3] == c) {
        return (char *)cp + 3;
    }
    if (cp[4] == c) {
        return (char *)cp + 4;
    }
    if (cp[5] == c) {
        return (char *)cp + 5;
    }
    if (cp[6] == c) {
        return (char *)cp + 6;
    }
    return (char *)cp + 7;
#else
    return (char *)cp + 3;
#endif
}

// --- Helper for gb_strstr ----------------------------------------------------

/**
 * @brief Byte-by-byte memory comparison of exactly n bytes.
 *
 * @param[in] s1 First memory block.
 * @param[in] s2 Second memory block.
 * @param[in] n  Number of bytes to compare.
 * @return 0 if equal, positive if s1 > s2, negative if s1 < s2.
 */
static inline int _memcmp_n(const void *s1, //
                            const void *s2, //
                            size_t      n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

// --- Helpers for gb_strncmp --------------------------------------------------

/**
 * @brief Byte-by-byte comparison loop until both pointers are word-aligned.
 *
 * Advances s1, s2, and n in place. Returns true and stores the comparison
 * result in *out if the strings differ or a null terminator is reached.
 * Returns false once both pointers are word-aligned with bytes still remaining
 * (caller should proceed to the word loop).
 *
 * @param[in,out] s1  Pointer-to-pointer for the first string.
 * @param[in,out] s2  Pointer-to-pointer for the second string.
 * @param[in,out] n   Remaining byte limit.
 * @param[out]    out Comparison result when return value is true.
 * @return true if a definitive result was found, false if alignment reached.
 */
static bool _strcmp_byte_phase(const char **s1, //
                               const char **s2, //
                               size_t      *n,  //
                               int         *out) {
    while ((*n > 0) && ((uintptr_t)*s1 & ALIGNMENT_MASK || (uintptr_t)*s2 & ALIGNMENT_MASK)) {
        unsigned char c1 = (unsigned char)**s1;
        unsigned char c2 = (unsigned char)**s2;
        if (c1 != c2 || c1 == '\0') {
            *out = (int)c1 - (int)c2;
            return true;
        }
        ++(*s1);
        ++(*s2);
        --(*n);
    }
    return false;
}

/**
 * @brief Byte-by-byte comparison tail loop after the word phase exits.
 *
 * Used when the word loop stops due to a suspected zero byte or mismatch
 * and byte-level resolution is needed to find the exact difference.
 *
 * @param[in] s1 Pointer to the first string position.
 * @param[in] s2 Pointer to the second string position.
 * @param[in] n  Remaining byte limit.
 * @return Lexicographic comparison result.
 */
static int _strcmp_tail_phase(const char *s1, //
                              const char *s2, //
                              size_t      n) {
    while ((n > 0) && (*s1 != '\0') && (*s1 == *s2)) {
        ++s1;
        ++s2;
        --n;
    }
    if (n == 0) {
        return 0;
    }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

// --- Helper for gb_strlen ----------------------------------------------------

/**
 * @brief Returns the byte offset of the first zero byte inside a word.
 *
 * The caller must have already confirmed that GB_HAS_ZERO(w) is true.
 * Isolating this endian-specific cascade in a helper keeps the hot gb_strlen
 * word loop free of branching on byte positions.
 *
 * On little-endian systems the least-significant byte is byte 0 of memory.
 * On big-endian systems the most-significant byte is byte 0.
 * The 64-bit path tests up to 8 byte positions; the 32-bit path tests 4.
 *
 * @param[in] w Word known to contain at least one zero byte.
 * @return Byte offset (0 – BYTES_PER_WORD-1) of the first zero byte.
 */
static inline size_t _zero_byte_offset(const gb_word_t w) {
    // clang-format off
#if GB_WORD_BITS == 64
  #if GB_BIG_ENDIAN
    if (!(w & 0xFF00000000000000ULL)) { return 0; }
    if (!(w & 0x00FF000000000000ULL)) { return 1; }
    if (!(w & 0x0000FF0000000000ULL)) { return 2; }
    if (!(w & 0x000000FF00000000ULL)) { return 3; }
    if (!(w & 0x00000000FF000000ULL)) { return 4; }
    if (!(w & 0x0000000000FF0000ULL)) { return 5; }
    if (!(w & 0x000000000000FF00ULL)) { return 6; }
  #else
    if (!(w & 0x00000000000000FFULL)) { return 0; }
    if (!(w & 0x000000000000FF00ULL)) { return 1; }
    if (!(w & 0x0000000000FF0000ULL)) { return 2; }
    if (!(w & 0x00000000FF000000ULL)) { return 3; }
    if (!(w & 0x000000FF00000000ULL)) { return 4; }
    if (!(w & 0x0000FF0000000000ULL)) { return 5; }
    if (!(w & 0x00FF000000000000ULL)) { return 6; }
  #endif
    return 7;
#else
  #if GB_BIG_ENDIAN
    if (!(w & 0xFF000000U)) { return 0; }
    if (!(w & 0x00FF0000U)) { return 1; }
    if (!(w & 0x0000FF00U)) { return 2; }
  #else
    if (!(w & 0x000000FFU)) { return 0; }
    if (!(w & 0x0000FF00U)) { return 1; }
    if (!(w & 0x00FF0000U)) { return 2; }
  #endif
    return 3;
#endif
    // clang-format on
}

// --- Helper for gb_dec2bin / gb_hex2bin / gb_dec2hex -------------------------

/**
 * @brief Returns the number of significant bits in a size_t value.
 *
 * Uses a binary-halving scan: each step tests whether the upper half of the
 * remaining value is non-zero, conditionally adds the half-width to the count,
 * and shifts the value down. This is O(log2(bits)) comparisons and produces
 * branchless cmov sequences on any modern compiler.
 *
 * Returns 1 for num == 0 so that the caller always emits at least one digit.
 * Safe on all platforms: no compiler builtins, no UB, no type-width assumptions.
 * The 64-bit half-step is guarded behind SIZE_MAX so 32-bit builds never
 * execute or see a shift-by-32 on a 32-bit type.
 *
 * @param[in] num Value whose significant bit count is needed.
 * @return Number of bits required to represent num (minimum 1).
 */
static inline int _count_sig_bits(size_t num) {
    int bits = 1;
    // clang-format off
#if SIZE_MAX > 0xFFFFFFFFUL
    if (num >> 32) { bits += 32; num >>= 32; }
#endif
    if (num >> 16) { bits += 16; num >>= 16; }
    if (num >>  8) { bits +=  8; num >>=  8; }
    if (num >>  4) { bits +=  4; num >>=  4; }
    if (num >>  2) { bits +=  2; num >>=  2; }
    if (num >>  1) { bits +=  1; }
    // clang-format on
    return bits;
}

/**
 * @brief Converts a size_t value to a zero-padded binary string.
 *
 * The number of significant bits is computed with _count_sig_bits, a portable
 * binary-halving scan. The output is then padded with leading '0' characters
 * to the next multiple of 8 bits before the binary digits are written from
 * least-significant to most-significant. The caller must ensure dst_bin is
 * large enough.
 *
 * @param[in]  num     Value to convert.
 * @param[out] dst_bin Destination buffer.
 * @param[in]  dst_len Size of destination buffer.
 * @return true on success, false if the buffer is too small.
 */
static bool _unsafe_dec2bin(const size_t num,     //
                            char        *dst_bin, //
                            const size_t dst_len) {
    int bits = _count_sig_bits(num);

    int rem = bits % 8;
    int pad = (rem > 0) ? (8 - rem) : 0;

    if ((pad + bits) < (int)dst_len) {
        for (int i = 0; i < pad; ++i) {
            dst_bin[i] = '0';
        }

        // Walk backwards from the MSB position — avoids recomputing
        // (pad + bits - 1 - i) on every iteration
        char *p = dst_bin + pad + bits - 1;
        for (int i = 0; i < bits; ++i, --p) {
            *p = (char)((num & (1UL << i)) ? '1' : '0');
        }

        dst_bin[pad + bits] = '\0';

        return true;
    }

    return false;
}

// *****************************************************************************
// *****************************************************************************
// Public Functions
// *****************************************************************************
// *****************************************************************************

// --- Memory — raw ---------------------------------------------------------

/**
 * @brief Copies a block of memory from source to destination.
 *
 * This function copies `len` bytes from the memory area pointed to by `src` to
 * the memory area pointed to by `dst`. It uses an 8-unrolled 32-bit word loop
 * to optimize the copying of memory.
 *
 * Algorithm: for short buffers (< 8 bytes) use a simple byte loop. For larger
 * transfers, copy byte-by-byte until dst reaches 4-byte alignment, then copy
 * 4 bytes at a time with an 8-unrolled loop, then handle the remaining tail
 * bytes individually.
 *
 * @param[out] dst Pointer to the destination memory area.
 * @param[in]  src Pointer to the source memory area.
 * @param[in]  len Number of bytes to copy.
 *
 * @return A pointer to the destination memory area `dst`.
 */
void *gb_memcpy(void *restrict dst,       //
                const void *restrict src, //
                size_t len) {
    char       *_dst = (char *)dst;
    const char *_src = (const char *)src;

    if (len < 8) {
        for (size_t i = 0; i < len; i++) {
            _dst[i] = _src[i];
        }
        return dst;
    }

    size_t i = 0;

    // Byte-copy until dst is word-aligned
    size_t align = (BYTES_PER_WORD - ((uintptr_t)_dst & ALIGNMENT_MASK)) & ALIGNMENT_MASK;
    if (align > len) {
        align = len;
    }
    for (; i < align; i++) {
        _dst[i] = _src[i];
    }

    // dst_aligned is guaranteed word-aligned by the head loop above;
    // src is read without an alignment requirement via the cast
    size_t      len_w       = (len - i) / BYTES_PER_WORD;
    char       *dst_aligned = _dst + i;
    const char *src_aligned = _src + i;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"

#pragma GCC unroll 8
    for (size_t j = 0; j < len_w; j++) {
        *(gb_word_t *)dst_aligned = *(const gb_word_t *)src_aligned;
        dst_aligned += BYTES_PER_WORD;
        src_aligned += BYTES_PER_WORD;
    }

#pragma GCC diagnostic pop

    // Tail bytes not covered by the word loop
    i += len_w * BYTES_PER_WORD;
    for (; i < len; i++) {
        _dst[i] = _src[i];
    }

    return dst;
}

/**
 * @brief Moves a block of memory from source to destination, handling overlaps.
 *
 * This function copies `len` bytes from the memory area pointed to by `src` to
 * the memory area pointed to by `dst`, correctly handling the case where the
 * two regions overlap.
 *
 * Algorithm: a backward copy is only required when dst falls strictly inside
 * [src, src+len), i.e. the destination lies ahead of the source within the
 * same region. In all other cases (no overlap, or dst < src) a forward copy
 * via gb_memcpy is safe and preferred.
 *
 * @param[out] dst Pointer to the destination memory area.
 * @param[in]  src Pointer to the source memory area.
 * @param[in]  len Number of bytes to move.
 *
 * @return A pointer to the destination memory area `dst`.
 */
void *gb_memmove(void       *dst, //
                 const void *src, //
                 size_t      len) {
    char       *_dst = (char *)dst;
    const char *_src = (const char *)src;

    if (_dst == _src || len == 0) {
        return dst;
    }

    if (_dst > _src && _dst < _src + len) {
        _memcpy_backward(_dst, _src, len);
    } else {
        gb_memcpy(dst, src, len);
    }

    return dst;
}

/**
 * @brief Sets a block of memory to a specified value.
 *
 * This function sets the first `len` bytes of the memory area pointed to by
 * `dst` to the specified value `val`. It uses an 8-unrolled 32-bit word loop
 * to optimize the setting of memory.
 *
 * Algorithm: the byte value is broadcast into all 4 lanes of a uint32_t via
 * multiplication by 0x01010101, producing a single word-wide fill value. The
 * same three-phase pattern as gb_memcpy is then used: head alignment, word
 * loop, tail bytes.
 *
 * @param[out] dst Pointer to the memory area to be set.
 * @param[in]  val The value to be set. The value is passed as an int, but the
 * function fills the memory block using the unsigned char conversion of this
 * value.
 * @param[in]  len Number of bytes to be set to the value.
 *
 * @return A pointer to the memory area `dst`.
 */
void *gb_memset(void  *dst, //
                int    val, //
                size_t len) {
    char *_dst = (char *)dst;

    if (len < 8) {
        for (size_t i = 0; i < len; i++) {
            _dst[i] = (char)val;
        }
        return dst;
    }

    size_t i = 0;

    // Byte-fill until dst is word-aligned
    size_t align = (BYTES_PER_WORD - ((uintptr_t)_dst & ALIGNMENT_MASK)) & ALIGNMENT_MASK;
    if (align > len) {
        align = len;
    }
    for (; i < align; i++) {
        _dst[i] = (char)val;
    }

    // Broadcast the fill byte into all lanes: e.g. 0xAB -> 0xABAB...AB
    gb_word_t val_word    = GB_BROADCAST_BYTE(val);
    size_t    len_w       = (len - i) / BYTES_PER_WORD;
    char     *dst_aligned = _dst + i;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"

#pragma GCC unroll 8
    for (size_t j = 0; j < len_w; j++) {
        *(gb_word_t *)dst_aligned = val_word;
        dst_aligned += BYTES_PER_WORD;
    }

#pragma GCC diagnostic pop

    // Tail bytes not covered by the word loop
    i += len_w * BYTES_PER_WORD;
    for (; i < len; i++) {
        _dst[i] = (char)val;
    }

    return dst;
}

/**
 * @brief Sets a block of memory to zero.
 *
 * Delegates to gb_memset(dst, 0, len) so that both functions share a single
 * optimized implementation.
 *
 * @param[out] dst Pointer to the memory block to be zeroed.
 * @param[in]  len Number of bytes to set to zero.
 */
void gb_bzero(void  *dst, //
              size_t len) {
    gb_memset(dst, 0, len);
}

/**
 * @brief Searches a memory block for the first occurrence of a byte value.
 *
 * Scans the first `len` bytes of the memory area pointed to by `buf` for the
 * first occurrence of `c` (interpreted as unsigned char).
 *
 * Algorithm: byte-by-byte until `buf` is word-aligned, then word-at-a-time
 * using GB_HAS_ZERO(w ^ cmask) to skip words with no match in two integer
 * operations, then a byte-level tail for any remaining bytes within the
 * length limit. The match-resolution byte scan within a matching word is
 * always at most BYTES_PER_WORD iterations and is a cold path.
 *
 * @param[in] buf Pointer to the memory area to search.
 * @param[in] c   Byte value to locate (passed as int, used as unsigned char).
 * @param[in] len Number of bytes to search.
 *
 * @return A pointer to the first occurrence of `c` in the first `len` bytes
 * of `buf`, or NULL if not found.
 */
void *gb_memchr(const void *buf, //
                int         c,   //
                size_t      len) {
    if (!buf || !len) {
        return NULL;
    }

    const unsigned char uc = (unsigned char)c;
    const char         *cp = (const char *)buf;

    // Byte-by-byte until cp is word-aligned
    while (len > 0 && ((uintptr_t)cp & ALIGNMENT_MASK)) {
        if ((unsigned char)*cp == uc) {
            return (void *)cp;
        }
        ++cp;
        --len;
    }

    if (len == 0) {
        return NULL;
    }

    gb_word_t cmask = GB_BROADCAST_BYTE(uc);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    const gb_word_t *wp = (const gb_word_t *)cp;
#pragma GCC diagnostic pop

    // Word-at-a-time scan — only consume full words within len
    while (len >= BYTES_PER_WORD) {
        gb_word_t w = *wp;
        if (GB_HAS_ZERO(w ^ cmask)) {
            // Match found in this word — resolve byte position
            const char *bp = (const char *)wp;
            for (size_t i = 0; i < BYTES_PER_WORD; ++i) {
                if ((unsigned char)bp[i] == uc) {
                    return (void *)(bp + i);
                }
            }
        }
        ++wp;
        len -= BYTES_PER_WORD;
    }

    // Tail bytes not covered by the word loop
    cp = (const char *)wp;
    while (len > 0) {
        if ((unsigned char)*cp == uc) {
            return (void *)cp;
        }
        ++cp;
        --len;
    }

    return NULL;
}

/**
 * @brief Searches a memory block for the last occurrence of a byte value.
 *
 * Scans the first `len` bytes of the memory area pointed to by `buf` for the
 * last occurrence of `c` (interpreted as unsigned char), scanning backwards
 * from byte `len - 1` down to byte 0.
 *
 * Algorithm: peel trailing bytes from the end until the scan pointer is
 * word-aligned, then word-at-a-time backwards using GB_HAS_ZERO(w ^ cmask),
 * then handle any remaining head bytes individually. Within a matching word
 * the bytes are resolved right-to-left to return the highest-addressed match.
 *
 * @param[in] buf Pointer to the memory area to search.
 * @param[in] c   Byte value to locate (passed as int, used as unsigned char).
 * @param[in] len Number of bytes to search.
 *
 * @return A pointer to the last occurrence of `c` in the first `len` bytes
 * of `buf`, or NULL if not found.
 */
void *gb_memrchr(const void *buf, //
                 int         c,   //
                 size_t      len) {
    if (!buf || !len) {
        return NULL;
    }

    const unsigned char uc = (unsigned char)c;
    const char         *cp = (const char *)buf + len; // one past end

    // Peel trailing bytes until cp is word-aligned
    while (len > 0 && ((uintptr_t)cp & ALIGNMENT_MASK)) {
        --cp;
        --len;
        if ((unsigned char)*cp == uc) {
            return (void *)cp;
        }
    }

    if (len == 0) {
        return NULL;
    }

    gb_word_t cmask = GB_BROADCAST_BYTE(uc);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    const gb_word_t *wp = (const gb_word_t *)cp;
#pragma GCC diagnostic pop

    // Word-at-a-time scan backwards
    while (len >= BYTES_PER_WORD) {
        --wp;
        len -= BYTES_PER_WORD;
        gb_word_t w = *wp;
        if (GB_HAS_ZERO(w ^ cmask)) {
            // Match in this word — resolve right-to-left for last occurrence
            const char *bp = (const char *)wp;
            for (int i = (int)BYTES_PER_WORD - 1; i >= 0; --i) {
                if ((unsigned char)bp[i] == uc) {
                    return (void *)(bp + i);
                }
            }
        }
    }

    // Remaining head bytes
    cp = (const char *)wp;
    while (len > 0) {
        --cp;
        --len;
        if ((unsigned char)*cp == uc) {
            return (void *)cp;
        }
    }

    return NULL;
}

// --- Memory — allocation --------------------------------------------------

/**
 * @brief Allocates an aligned block of memory.
 *
 * Allocates `size` bytes of memory whose start address is a multiple of
 * `align`. The alignment must be a non-zero power of two and at most 128;
 * if it is smaller than `sizeof(void *)` it is silently raised to that
 * minimum so the returned pointer is always safely pointer-aligned.
 *
 * Implementation: a single `malloc` call allocates `size + align` bytes
 * (the extra `align` bytes provide the worst-case padding budget). The
 * payload is then bumped forward to the next `align`-boundary, and the
 * byte immediately before the payload records the total back-offset from
 * the payload to the raw block (range 1–128, fits in uint8_t with no
 * wrapping). `gb_free` reads that byte and subtracts it to recover the
 * original pointer before calling `free`.
 *
 * @param[in] size  Number of bytes to allocate.
 * @param[in] align Alignment in bytes (must be a power of two, ≤ 128).
 *
 * @return Pointer to the aligned memory block, or NULL on failure (invalid
 * alignment, zero size, or allocation failure).
 */
void *gb_malloc(size_t size, //
                size_t align) {
    if (!size || !GB_IS_POWER_OF_2(align) || align > 128) {
        return NULL;
    }
    if (align < sizeof(void *)) {
        align = sizeof(void *);
    }

    // Overflow guard: size + align + 1 must not wrap around.
    // align ≤ 128 so align + 1 ≤ 129; check size separately.
    if (size > SIZE_MAX - align - 1U) {
        return NULL;
    }

    // Allocate raw block: payload size + one full align worth of padding
    // budget + 1 byte reserved for the back-offset field
    uint8_t *raw = (uint8_t *)malloc(size + align + 1U);
    if (!raw) {
        return NULL;
    }

    // Reserve 1 byte before the payload for the back-offset field, then
    // advance to the next align-boundary
    uint8_t  *base = raw + 1U;
    uintptr_t mod  = (uintptr_t)base & (align - 1U);
    size_t    pad  = (mod == 0U) ? 0U : (align - (size_t)mod);
    uint8_t  *ptr  = base + pad;

    // back_offset = ptr − raw ∈ [1, align] ⊆ [1, 128] — fits in uint8_t
    uint8_t back_offset = (uint8_t)(ptr - raw);
    ptr[-1]             = back_offset;

    return (void *)ptr;
}

/**
 * @brief Frees a block allocated by gb_malloc.
 *
 * Reads the back-offset byte stored immediately before `ptr` to recover the
 * original raw pointer passed to `malloc`, then calls `free` on it.
 *
 * @warning Passing a pointer not obtained from `gb_malloc` (including
 * pointers from plain `malloc`, `calloc`, or `realloc`) is undefined
 * behavior and will corrupt the heap.
 *
 * @param[in] ptr Pointer previously returned by gb_malloc, or NULL (no-op).
 */
void gb_free(void *ptr) {
    if (!ptr) {
        return;
    }

    uint8_t *p   = (uint8_t *)ptr;
    uint8_t *raw = p - p[-1]; // recover raw = ptr − back_offset

    free(raw);
}

// --- String — length ------------------------------------------------------

/**
 * @brief Calculates the length of a string.
 *
 * This function calculates the length of the null-terminated string `str`.
 *
 * Algorithm: after aligning cp to a 4-byte boundary byte-by-byte, reads the
 * string one word at a time. GB_HAS_ZERO detects a zero byte in two integer
 * operations; when found, _zero_byte_offset pinpoints the exact byte within
 * the word to compute the final length.
 *
 * @param[in] str Pointer to the string.
 *
 * @return The length of the string, excluding the null terminator.
 */
size_t gb_strlen(const char *str) {
    if (!str) {
        return (size_t)0;
    }

    const char *cp = str;

    // Advance byte-by-byte until cp is word-aligned
    while ((uintptr_t)cp & ALIGNMENT_MASK) {
        // clang-format off
        if (!*cp) { return (size_t)(cp - str); }
        // clang-format on
        ++cp;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    const gb_word_t *wp = (const gb_word_t *)cp;
#pragma GCC diagnostic pop

    while (1) {
        gb_word_t w = *wp;
        if (GB_HAS_ZERO(w)) {
            return (uintptr_t)wp - (uintptr_t)str + _zero_byte_offset(w);
        }
        ++wp;
    }
}

/**
 * @brief Calculates the length of a string, up to a maximum limit.
 *
 * Returns the number of bytes in the null-terminated string `str`, not
 * including the terminating null character, but at most `maxlen`. If no null
 * terminator is found within the first `maxlen` bytes, `maxlen` is returned.
 *
 * Algorithm: identical word-at-a-time strategy as gb_strlen — byte-by-byte
 * head alignment, GB_HAS_ZERO word scan, _zero_byte_offset resolution — but
 * the word loop only consumes full words that fit within `maxlen`, and a
 * byte-level tail handles any remaining bytes up to the cap.
 *
 * @param[in] str    Pointer to the string.
 * @param[in] maxlen Maximum number of bytes to examine.
 *
 * @return The length of `str` if it is shorter than `maxlen`, otherwise
 * `maxlen`.
 */
size_t gb_strnlen(const char *str, //
                  size_t      maxlen) {
    if (!str || !maxlen) {
        return 0;
    }

    const char *cp = str;

    // Byte-by-byte until cp is word-aligned, respecting maxlen
    size_t remaining = maxlen;
    while ((remaining > 0) && ((uintptr_t)cp & ALIGNMENT_MASK)) {
        if (*cp == '\0') {
            return (size_t)(cp - str);
        }
        ++cp;
        --remaining;
    }

    if (remaining == 0) {
        return maxlen;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    const gb_word_t *wp = (const gb_word_t *)cp;
#pragma GCC diagnostic pop

    // Word-at-a-time — only consume full words within maxlen
    while (remaining >= BYTES_PER_WORD) {
        gb_word_t w = *wp;
        if (GB_HAS_ZERO(w)) {
            return (uintptr_t)wp - (uintptr_t)str + _zero_byte_offset(w);
        }
        ++wp;
        remaining -= BYTES_PER_WORD;
    }

    // Tail bytes not covered by the word loop
    cp = (const char *)wp;
    while (remaining > 0) {
        if (*cp == '\0') {
            return (size_t)(cp - str);
        }
        ++cp;
        --remaining;
    }

    return maxlen;
}

// --- String — copy / concat -----------------------------------------------

/**
 * @brief Copies a string from source to destination.
 *
 * This function copies the string pointed to by `src` (including the null
 * terminator) to the memory area pointed to by `dst`.
 *
 * @warning This function does not perform bounds checking. If the destination
 * buffer is not large enough to hold the source string, a buffer overflow will
 * occur, leading to undefined behavior and potential security vulnerabilities.
 * Use with extreme caution.
 *
 * Algorithm: dst is aligned byte-by-byte to a 4-byte boundary. src is then
 * read word-at-a-time via gb_memcpy into a local uint32_t, which handles any
 * src misalignment safely on all targets. Words without a null byte are
 * written directly; once a null is detected the remaining bytes are copied
 * individually including the terminator.
 *
 * @param[out] dst Pointer to the destination buffer.
 * @param[in]  src Pointer to the source string.
 *
 * @return A pointer to the destination buffer.
 */
char *gb_strcpy(char *restrict dst, //
                const char *restrict src) {
    if (!dst || !src) {
        return dst;
    }

    char *ptr = dst;

    // Align dst to a word boundary one byte at a time
    while ((uintptr_t)ptr & ALIGNMENT_MASK) {
        *ptr = *src;
        if (*ptr == '\0') {
            return dst;
        }
        ++ptr;
        ++src;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    gb_word_t *wp = (gb_word_t *)ptr;
#pragma GCC diagnostic pop

    while (1) {
        gb_word_t w;
        gb_memcpy(&w, src, sizeof(w)); // safe unaligned read from src
        if (GB_HAS_ZERO(w)) {
            break;
        }
        *wp++ = w;
        src += sizeof(gb_word_t);
    }

    // Copy the final bytes (including the null terminator)
    ptr = (char *)wp;
    do {
        *ptr = *src;
        ++ptr;
        ++src;
    } while (*(ptr - 1) != '\0');

    return dst;
}

/**
 * @brief Copies up to `n` characters from a source string to a destination
 * buffer.
 *
 * This function copies up to `n` characters from the string pointed to by `src`
 * to the memory area pointed to by `dst`.
 *
 * Unlike the standard `strncpy`, this implementation **always** ensures the
 * destination string is null-terminated, providing a safer alternative.
 *
 * Algorithm: the head loop advances until both pointers are 4-byte aligned.
 * The word loop then copies 4 bytes at a time while n >= 4 and no null byte
 * is detected in the source word. The tail loop handles the remaining bytes
 * followed by an unconditional null terminator write.
 *
 * @param[out] dst Pointer to the destination buffer.
 * @param[in]  src Pointer to the source string.
 * @param[in]  n   The maximum number of characters to copy from `src`.
 *
 * @return A pointer to the destination buffer.
 */
char *gb_strncpy(char *restrict dst,       //
                 const char *restrict src, //
                 size_t n) {
    if (!dst || !src || !n) {
        return dst;
    }

    char *ptr = dst;

    // Advance byte-by-byte until both pointers are word-aligned
    while ((n > 0) && (*src) &&                  //
           (((uintptr_t)ptr & ALIGNMENT_MASK) || //
            ((uintptr_t)src & ALIGNMENT_MASK))) {
        *ptr = *src;

        ++ptr;
        ++src;
        --n;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    gb_word_t       *wp1 = (gb_word_t *)ptr;
    const gb_word_t *wp2 = (const gb_word_t *)src;
#pragma GCC diagnostic pop

    while ((n >= sizeof(gb_word_t)) && !GB_HAS_ZERO(*wp2)) {
        *wp1 = *wp2;

        ++wp1;
        ++wp2;
        n -= sizeof(gb_word_t);
    }

    ptr = (char *)wp1;
    src = (const char *)wp2;

    while ((n > 0) && *src) {
        *ptr = *src;

        ++ptr;
        ++src;
        --n;
    }

    // Always null-terminate, even when src was truncated
    *ptr = '\0';

    return dst;
}

/**
 * @brief Copies a string to a destination buffer with size-bound and
 * guaranteed null termination.
 *
 * Copies at most `size - 1` characters from `src` to `dst`, always appending
 * a null terminator if `size` is greater than zero. The destination is never
 * over-written beyond `size` bytes total.
 *
 * Unlike gb_strcpy (which has no bound) and gb_strncpy (which may leave the
 * destination unterminated), this function always produces a valid C string
 * and reports the full source length so the caller can detect truncation.
 *
 * @note Truncation occurred if the return value is >= `size`.
 *
 * @param[out] dst  Pointer to the destination buffer.
 * @param[in]  src  Pointer to the source string.
 * @param[in]  size Total size of the destination buffer in bytes.
 *
 * @return The length of `src` (as if computed by gb_strlen), regardless of
 * truncation. Allows the caller to detect truncation by comparing the return
 * value against `size`.
 */
size_t gb_strlcpy(char *restrict dst,       //
                  const char *restrict src, //
                  size_t size) {
    size_t src_len = gb_strlen(src);

    if (size > 0) {
        size_t copy_len = (src_len < size - 1) ? src_len : size - 1;
        gb_memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }

    return src_len;
}

/**
 * @brief Appends a string to a destination buffer with size-bound and
 * guaranteed null termination.
 *
 * Appends characters from `src` to the end of `dst`, ensuring that the total
 * length of the resulting string never exceeds `size - 1` characters (plus
 * the null terminator). The destination is always null-terminated if `size`
 * is greater than zero and `dst` was already null-terminated within `size`
 * bytes.
 *
 * @note Truncation occurred if the return value is >= `size`.
 *
 * @param[in,out] dst  Pointer to the destination buffer (null-terminated).
 * @param[in]     src  Pointer to the source string to append.
 * @param[in]     size Total size of the destination buffer in bytes.
 *
 * @return The total length that would result from a fully un-truncated
 * concatenation: gb_strlen(initial dst) + gb_strlen(src). Allows the caller
 * to detect truncation by comparing the return value against `size`.
 */
size_t gb_strlcat(char *restrict dst,       //
                  const char *restrict src, //
                  size_t size) {
    size_t dst_len = gb_strnlen(dst, size);
    size_t src_len = gb_strlen(src);

    if (dst_len < size) {
        size_t avail    = size - dst_len - 1;
        size_t copy_len = (src_len < avail) ? src_len : avail;
        gb_memcpy(dst + dst_len, src, copy_len);
        dst[dst_len + copy_len] = '\0';
    }

    return dst_len + src_len;
}

/**
 * @brief Duplicates a string into a newly allocated buffer.
 *
 * Allocates exactly gb_strlen(str) + 1 bytes, copies the string content with
 * gb_memcpy, and appends the null terminator. The returned pointer must be
 * freed by the caller.
 *
 * @param[in] str Pointer to the null-terminated string to duplicate.
 *
 * @return Pointer to the newly allocated duplicate string, or NULL if
 * allocation fails or `str` is NULL.
 */
char *gb_strdup(const char *str) {
    if (!str) {
        return NULL;
    }

    size_t len = gb_strlen(str);

    if (len == SIZE_MAX) { // len + 1 would wrap
        return NULL;
    }

    char *dup = (char *)malloc(len + 1);
    if (!dup) {
        return NULL;
    }

    gb_memcpy(dup, str, len);
    dup[len] = '\0';

    return dup;
}

// --- String — search ------------------------------------------------------

/**
 * @brief Finds the first occurrence of a character in a string.
 *
 * This function searches for the first occurrence of the character `c` in the
 * string `str`. If `c` is the null character '\0', it returns a pointer to the
 * terminating null character.
 *
 * Algorithm: after aligning to a 4-byte boundary byte-by-byte, the target
 * byte is broadcast into all 4 lanes of a uint32_t mask (cmask). Each word is
 * tested with GB_HAS_ZERO: first on the word itself to detect end-of-string,
 * then on (word XOR cmask) to detect a match — a matching byte becomes 0x00,
 * which GB_HAS_ZERO detects in two integer operations.
 *
 * @param[in] str Pointer to the null-terminated string to search.
 * @param[in] c   The character to locate (passed as an int).
 *
 * @return A pointer to the first occurrence of `c` in `str`, or `NULL` if the
 * character is not found.
 */
char *gb_strchr(const char *str, //
                int         c) {
    if (!str) {
        return NULL;
    }

    char *cp = (char *)str; // NOSONAR (drop const qualifier)

    if (c == 0) {
        return (char *)str + gb_strlen(str);
    }

    // Byte-by-byte until cp is word-aligned
    while ((uintptr_t)cp & ALIGNMENT_MASK) {
        if (*cp == '\0') {
            return NULL;
        }
        if (*cp == (char)c) {
            return cp;
        }
        ++cp;
    }

    // Broadcast target byte into all lanes: XOR-ing a word with cmask
    // turns any matching byte into 0x00, detectable by GB_HAS_ZERO
    gb_word_t cmask = GB_BROADCAST_BYTE(c);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    gb_word_t *wp = (gb_word_t *)cp;
#pragma GCC diagnostic pop

    while (1) {
        gb_word_t w = *wp;
        if (GB_HAS_ZERO(w)) {
            return _strchr_in_zero_word((char *)wp, (char)c);
        }
        if (GB_HAS_ZERO(w ^ cmask)) {
            return _strchr_in_match_word((char *)wp, (char)c);
        }
        ++wp;
    }
}

/**
 * @brief Finds the last occurrence of a character in a string.
 *
 * This function searches for the last occurrence of the character `c` in the
 * string `str`. If `c` is the null character '\0', it returns a pointer to the
 * terminating null character.
 *
 * Algorithm: the string is scanned in one forward pass so that last_occurrence
 * always holds the most recent match seen so far. The head bytes (before word
 * alignment) are handled individually. The target byte is then broadcast into
 * all lanes of a gb_word_t mask (cmask) via GB_BROADCAST_BYTE. Each aligned
 * word is loaded once into a local register and tested with GB_HAS_ZERO: first
 * on the word itself to detect end-of-string, then on (word XOR cmask) to
 * detect a match — a matching byte becomes 0x00, which GB_HAS_ZERO detects in
 * two integer operations, skipping _search_char_in_word entirely on words that
 * contain no match. When a zero word is found, its bytes are resolved
 * individually to pick up any final match before the null terminator.
 *
 * @param[in] str Pointer to the null-terminated string to search.
 * @param[in] c   The character to locate (passed as an int).
 *
 * @return A pointer to the last occurrence of `c` in `str`, or `NULL` if the
 * character is not found.
 */
char *gb_strrchr(const char *str, //
                 int         c) {
    if (!str) {
        return NULL;
    }

    char *last_occurrence = NULL;
    char *cp              = (char *)str; // NOSONAR (drop const qualifier)

    if (c == 0) {
        return (char *)str + gb_strlen(str);
    }

    // Handle unaligned head bytes, updating last_occurrence along the way
    if (_process_unaligned_bytes(&cp, c, &last_occurrence)) {
        return last_occurrence;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    gb_word_t *wp = (gb_word_t *)cp;
#pragma GCC diagnostic pop

    // Broadcast target byte into all lanes: XOR-ing a word with cmask
    // turns any matching byte into 0x00, detectable by GB_HAS_ZERO
    gb_word_t cmask = GB_BROADCAST_BYTE(c);

    while (1) {
        gb_word_t word = *wp; // single load — kept in register across both checks

        // When a null terminator is detected, resolve individual bytes
        // to collect any final match, then stop
        if (GB_HAS_ZERO(word)) {
            _process_word_with_zero(wp, c, &last_occurrence);
            return last_occurrence;
        }

        // Fast-path: skip the helper call entirely if no byte in this
        // word equals c — XOR zeros the matching lane(s), GB_HAS_ZERO
        // detects them in a branchless bitmask test
        if (GB_HAS_ZERO(word ^ cmask)) {
            _search_char_in_word(wp, c, &last_occurrence);
        }

        ++wp;
    }
}

/**
 * @brief Finds the first occurrence of a substring in a string.
 *
 * This function searches for the first occurrence of the substring `needle`
 * in the string `haystack`. If `needle` is an empty string, it returns a
 * pointer to `haystack`.
 *
 * Algorithm: gb_strlen is called only on needle, not on haystack, because
 * measuring haystack upfront would traverse the entire string before any
 * matching begins. For a single-character needle gb_strchr is used directly.
 * For longer needles, gb_strchr advances to each first-character candidate
 * word-at-a-time; _memcmp_n then confirms whether the full needle matches at
 * that position, avoiding the per-hit alignment overhead of a byte-by-byte scan.
 *
 * @param[in] haystack Pointer to the null-terminated string to search in.
 * @param[in] needle   Pointer to the null-terminated substring to find.
 *
 * @return A pointer to the first occurrence of `needle` in `haystack`, or
 * `NULL` if the substring is not found.
 */
char *gb_strstr(const char *haystack, //
                const char *needle) {
    if (!haystack || !needle) {
        return NULL;
    }

    if (*needle == 0) {
        return (char *)haystack;
    }

    if (*haystack == 0) {
        return NULL;
    }

    size_t needle_len = gb_strlen(needle);

    if (needle_len == 1) {
        return gb_strchr(haystack, *needle);
    }

    const char *h  = haystack;
    char        fc = needle[0];

    // Use gb_strchr to skip to each candidate first-character position
    // word-at-a-time rather than advancing one byte at a time
    while ((h = gb_strchr(h, fc)) != NULL) {
        if (_memcmp_n(h, needle, needle_len) == 0) {
            return (char *)h;
        }
        ++h;
    }

    return NULL;
}

/**
 * @brief Finds the first occurrence of any character from a set of characters.
 *
 * This function scans the null-terminated string `str` for the first
 * occurrence of any character from the null-terminated string `reject`.
 *
 * Algorithm: a 256-entry boolean lookup table is built from reject in O(m),
 * then str is scanned in O(n) with O(1) per-character membership tests,
 * for an overall O(n + m) cost regardless of reject length.
 *
 * @note Returns 0 on NULL input. Standard strcspn has undefined behavior on
 * NULL; this function returns 0 defensively in that case.
 *
 * @param[in] str    Pointer to the null-terminated string to search.
 * @param[in] reject Pointer to the null-terminated string containing characters to reject.
 *
 * @return The number of characters at the beginning of `str` that are not
 * in the `reject` string.
 */
size_t gb_strcspn(const char *str, //
                  const char *reject) {
    if (!str || !reject) {
        return 0;
    }

    uint8_t lookup[256];
    gb_memset(lookup, 0, sizeof(lookup));

    const char *r = reject;
    while (*r) {
        lookup[(unsigned char)*r] = 1;
        ++r;
    }

    const char *p = str;
    while (*p) {
        if (lookup[(unsigned char)*p]) {
            break;
        }
        ++p;
    }

    return (size_t)(p - str);
}

/**
 * @brief Splits a string into tokens using delimiters.
 *
 * This function parses the string `str` into a sequence of tokens, separated
 * by one or more characters from the string `delim`. The `saveptr` argument
 * maintains state between calls to continue tokenization of the same string.
 *
 * This is a thread-safe implementation equivalent to the standard strtok_r.
 *
 * Algorithm: a 256-entry lookup table is built from delim once per call,
 * giving O(1) delimiter membership tests and reducing the overall cost from
 * O(n*m) to O(n + m). The scan then skips leading delimiters, marks the
 * token start, advances to the next delimiter, null-terminates in place,
 * and saves the resume position in saveptr.
 *
 * @param[in]     str     String to tokenize (NULL to continue with previous string).
 * @param[in]     delim   String containing delimiter characters.
 * @param[in,out] saveptr Pointer to save parsing state between calls.
 *
 * @return Pointer to the next token, or NULL if no more tokens exist.
 */
char *gb_strtok_r(char *restrict str,         //
                  const char *restrict delim, //
                  char **restrict saveptr) {
    if (!delim || !saveptr) {
        return NULL;
    }

    if (str != NULL) {
        *saveptr = str;
    }

    char *current = *saveptr;
    if (current == NULL) {
        return NULL;
    }

    // Build a 256-entry lookup table from delim for O(1) membership tests
    uint8_t lookup[256];
    gb_memset(lookup, 0, sizeof(lookup));
    for (const char *d = delim; *d; ++d) {
        lookup[(unsigned char)*d] = 1;
    }

    // Skip leading delimiter characters
    while (*current && lookup[(unsigned char)*current]) {
        current++;
    }

    if (*current == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    // Advance to the first delimiter after the token
    char *token_start = current;
    while (*current && !lookup[(unsigned char)*current]) {
        current++;
    }

    // Null-terminate the token in place and advance past the delimiter
    if (*current != '\0') {
        *current = '\0';
        current++;
    }

    *saveptr = current;
    return token_start;
}

// --- String — compare -----------------------------------------------------

/**
 * @brief Compares two strings lexicographically.
 *
 * Delegates to gb_strncmp with SIZE_MAX so that both functions share a single
 * implementation with no artificial byte limit.
 *
 * @param[in] str1 Pointer to the first string.
 * @param[in] str2 Pointer to the second string.
 *
 * @return An integer less than, equal to, or greater than zero if `str1` is
 * found, respectively, to be less than, to match, or be greater than `str2`.
 */
int gb_strcmp(const char *str1, //
              const char *str2) {
    return gb_strncmp(str1, str2, SIZE_MAX);
}

/**
 * @brief Compares up to `n` characters of two strings lexicographically.
 *
 * This function compares up to `n` characters of the two null-terminated
 * strings `str1` and `str2`.
 *
 * Algorithm: three phases. (1) Byte phase — advances both pointers to word
 * alignment, returning immediately on any difference or null. (2) Word phase —
 * XOR of two words is zero only if all bytes are identical; GB_HAS_ZERO on the
 * XOR result detects a mismatch, and GB_HAS_ZERO on w1 alone detects a null
 * (equal words share the same zero bytes so checking w1 suffices). (3) Tail
 * phase — resolves the exact differing byte after the word loop exits.
 *
 * @param[in] str1 Pointer to the first string.
 * @param[in] str2 Pointer to the second string.
 * @param[in] n    The maximum number of characters to compare.
 *
 * @return An integer less than, equal to, or greater than zero if the first `n`
 * bytes of `str1` is found, respectively, to be less than, to match, or be
 * greater than the first `n` bytes of `str2`.
 */
int gb_strncmp(const char *str1, //
               const char *str2, //
               size_t      n) {
    if (n == 0) {
        return 0;
    }
    // same pointer (also covers both NULL)
    if (str1 == str2) {
        return 0;
    }
    if (!str1) {
        return -1;
    }
    if (!str2) {
        return 1;
    }

    // Phase 1: byte-by-byte until both pointers are word-aligned
    int out;
    if (_strcmp_byte_phase(&str1, &str2, &n, &out)) {
        return out;
    }

    if (n == 0) {
        return 0;
    }

    // Phase 2: word-at-a-time; XOR detects any difference in one operation
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    const gb_word_t *wp1 = (const gb_word_t *)str1;
    const gb_word_t *wp2 = (const gb_word_t *)str2;
#pragma GCC diagnostic pop

    while (n >= sizeof(gb_word_t)) {
        gb_word_t w1 = *wp1;
        gb_word_t w2 = *wp2;
        if ((w1 ^ w2) != 0U || GB_HAS_ZERO(w1)) {
            break;
        }
        ++wp1;
        ++wp2;
        n -= sizeof(gb_word_t);
    }

    if (n == 0 || *wp1 == *wp2) {
        return 0;
    }

    // Phase 3: resolve the exact differing byte
    return _strcmp_tail_phase((const char *)wp1, (const char *)wp2, n);
}

// --- Conversion -----------------------------------------------------------

/**
 * @brief Converts a binary string to a decimal string.
 *
 * This function converts a binary number represented as a string `src_bin` into
 * a decimal string `dst_dec`.
 *
 * @param[in]  src_bin Pointer to the source binary string.
 * @param[out] dst_dec Pointer to the destination decimal string.
 * @param[in]  dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_bin2dec(const char *restrict src_bin, //
                char *restrict dst_dec,       //
                size_t dst_len) {
    bool rvalue = (src_bin && dst_dec && dst_len);

    if (rvalue) {
        char *err;
        errno      = 0;
        size_t num = strtoul(src_bin, &err, 2);

        rvalue = !(*err) && (errno != ERANGE);

        if (rvalue) {
            rvalue = snprintf(dst_dec, dst_len, "%zu", num) > 0;
        }
    }

    return rvalue;
}

/**
 * @brief Converts a binary string to a hexadecimal string.
 *
 * This function converts a binary number represented as a string `src_bin` into
 * a hexadecimal string `dst_hex`.
 *
 * @param[in]  src_bin Pointer to the source binary string.
 * @param[out] dst_hex Pointer to the destination hexadecimal string.
 * @param[in]  dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_bin2hex(const char *restrict src_bin, //
                char *restrict dst_hex,       //
                size_t dst_len) {
    bool rvalue = (src_bin && dst_hex && dst_len);

    if (rvalue) {
        char *err;
        errno      = 0;
        size_t num = strtoul(src_bin, &err, 2);

        rvalue = !(*err) && (errno != ERANGE);

        if (rvalue) {
            rvalue = snprintf(dst_hex, dst_len, "%zX", num) > 0;
        }
    }

    return rvalue;
}

/**
 * @brief Converts a decimal string to a binary string.
 *
 * This function converts a decimal number represented as a string `src_dec`
 * into a binary string `dst_bin`. The output is padded with leading zeros to
 * ensure it is a multiple of 8 characters.
 *
 * @param[in]  src_dec Pointer to the source decimal string.
 * @param[out] dst_bin Pointer to the destination binary string.
 * @param[in]  dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_dec2bin(const char *restrict src_dec, //
                char *restrict dst_bin,       //
                size_t dst_len) {
    bool rvalue = (src_dec && dst_bin && dst_len);

    if (rvalue) {
        char *err;
        errno      = 0;
        size_t num = strtoul(src_dec, &err, 10);

        rvalue = !(*err) && (errno != ERANGE);

        if (rvalue) {
            rvalue = _unsafe_dec2bin(num, dst_bin, dst_len);
        }
    }

    return rvalue;
}

/**
 * @brief Converts a decimal string to a hexadecimal string.
 *
 * This function converts a decimal number represented as a string `src_dec`
 * into a hexadecimal string `dst_hex`. The output is padded with leading zeros
 * to ensure it is a multiple of 4 characters.
 *
 * Algorithm: snprintf writes the raw hex representation; its return value is
 * validated against dst_len before the padding step to ensure the buffer is
 * large enough for the shift. If the resulting length is not already a multiple
 * of 4, the existing digits are shifted right and the gap is filled with '0'.
 *
 * @param[in]  src_dec Pointer to the source decimal string.
 * @param[out] dst_hex Pointer to the destination hexadecimal string.
 * @param[in]  dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_dec2hex(const char *restrict src_dec, //
                char *restrict dst_hex,       //
                size_t dst_len) {
    bool rvalue = (src_dec && dst_hex && dst_len);

    if (!rvalue) {
        return false;
    }

    char *err;
    errno      = 0;
    size_t num = strtoul(src_dec, &err, 10);

    rvalue = !(*err) && (errno != ERANGE);

    if (rvalue) {
        // snprintf returns the number of characters written; reuse that
        // value as len directly — avoids a redundant gb_strlen traversal
        int written = snprintf(dst_hex, dst_len, "%zX", num);
        rvalue      = (written > 0 && (size_t)written < dst_len);

        if (rvalue) {
            int len = written;
            int rem = len % 4;
            int pad = 4 - rem;

            // Pad to a multiple of 4 hex digits: shift existing digits right,
            // then fill the vacated leading positions with '0'
            if (rem > 0 && ((pad + len) < (int)dst_len)) {
                for (int i = len; i >= 0; --i) {
                    dst_hex[i + pad] = dst_hex[i];
                }

                for (int i = 0; i < pad; ++i) {
                    dst_hex[i] = '0';
                }
            }
        }
    }

    return rvalue;
}

/**
 * @brief Converts a hexadecimal string to a binary string.
 *
 * This function converts a hexadecimal number represented as a string `src_hex`
 * into a binary string `dst_bin`. The output is padded with leading zeros to
 * ensure it is a multiple of 8 characters.
 *
 * @param[in]  src_hex Pointer to the source hexadecimal string.
 * @param[out] dst_bin Pointer to the destination binary string.
 * @param[in]  dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2bin(const char *restrict src_hex, //
                char *restrict dst_bin,       //
                size_t dst_len) {
    bool rvalue = (src_hex && dst_bin && dst_len);

    if (rvalue) {
        char *err;
        errno      = 0;
        size_t num = strtoul(src_hex, &err, 16);

        rvalue = !(*err) && (errno != ERANGE);

        if (rvalue) {
            rvalue = _unsafe_dec2bin(num, dst_bin, dst_len);
        }
    }

    return rvalue;
}

/**
 * @brief Converts a hexadecimal string to a decimal string.
 *
 * This function converts a hexadecimal number represented as a string `src_hex`
 * into a decimal string `dst_dec`.
 *
 * @param[in]  src_hex Pointer to the source hexadecimal string.
 * @param[out] dst_dec Pointer to the destination decimal string.
 * @param[in]  dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2dec(const char *restrict src_hex, //
                char *restrict dst_dec,       //
                size_t dst_len) {
    bool rvalue = (src_hex && dst_dec && dst_len);

    if (rvalue) {
        char *err;
        errno      = 0;
        size_t num = strtoul(src_hex, &err, 16);

        rvalue = !(*err) && (errno != ERANGE);

        if (rvalue) {
            rvalue = snprintf(dst_dec, dst_len, "%zu", num) > 0;
        }
    }

    return rvalue;
}

/**
 * @brief Converts a raw binary byte buffer to its hexadecimal string
 * representation.
 *
 * This function takes a buffer of raw bytes and converts it into a
 * null-terminated string of hexadecimal characters. Each byte in the source
 * buffer is represented by two characters in the destination string.
 *
 * Algorithm: each source byte is split into its high nibble (bits 7–4) and
 * low nibble (bits 3–0) and each nibble is used as an index into the
 * hex_chars lookup table to produce the two output characters.
 *
 * @param[in]  src_buf Pointer to the source buffer containing the raw binary data.
 * @param[in]  src_len The number of bytes in the source buffer to convert.
 * @param[out] dst_str Pointer to the destination buffer where the resulting
 * hexadecimal string will be stored.
 * @param[in]  dst_len The size of the destination buffer in bytes. Must be at
 * least (src_len * 2) + 1 to accommodate the full string and the null terminator.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2str(const char *restrict src_buf, //
                size_t src_len,               //
                char *restrict dst_str,       //
                size_t dst_len) {
    bool rvalue = (src_buf && src_len && dst_str && dst_len > src_len * 2);

    if (rvalue) {
        const char hex_chars[] = "0123456789ABCDEF";

        for (size_t i = 0; i < src_len; ++i) {
            dst_str[(i * 2) + 0] = hex_chars[((unsigned char)src_buf[i] >> 4) & 0x0F];
            dst_str[(i * 2) + 1] = hex_chars[((unsigned char)src_buf[i] >> 0) & 0x0F];
        }

        dst_str[src_len * 2] = '\0';
    }

    return rvalue;
}

/**
 * @brief Converts a raw binary byte buffer to its hexadecimal string
 * representation, with byte reversal.
 *
 * Same nibble-splitting algorithm as gb_hex2str, but bytes are read from
 * last to first (index src_len-1-i), producing a byte-reversed
 * (endian-swapped) output string.
 *
 * For example, the input buffer {0x1A, 0x2B, 0x3C} yields "3C2B1A".
 *
 * @param[in]  src_buf Pointer to the source buffer containing the raw binary data.
 * @param[in]  src_len The number of bytes in the source buffer to convert.
 * @param[out] dst_str Pointer to the destination buffer where the resulting
 * hexadecimal string will be stored.
 * @param[in]  dst_len The size of the destination buffer in bytes. Must be at
 * least (src_len * 2) + 1 to accommodate the full string and the null terminator.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2str_r(const char *restrict src_buf, //
                  size_t src_len,               //
                  char *restrict dst_str,       //
                  size_t dst_len) {
    bool rvalue = (src_buf && src_len && dst_str && dst_len > src_len * 2);

    if (rvalue) {
        const char hex_chars[] = "0123456789ABCDEF";

        for (size_t i = 0; i < src_len; ++i) {
            dst_str[(i * 2) + 0] = hex_chars[((unsigned char)src_buf[src_len - 1 - i] >> 4) & 0x0F];
            dst_str[(i * 2) + 1] = hex_chars[((unsigned char)src_buf[src_len - 1 - i] >> 0) & 0x0F];
        }

        dst_str[src_len * 2] = '\0';
    }

    return rvalue;
}

/* *****************************************************************************
 End of File
 */
