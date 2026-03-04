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

#include <stdint.h> // SIZE_MAX, uint32_t, uintptr_t
#include <stdio.h>  // size_t, snprintf
#include <stdlib.h> // NULL, strtoul

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

/**
 * @brief Helper function to process unaligned bytes for strrchr.
 *
 * @param[in,out] cp Pointer to current position (updated)
 * @param[in] c Character to find
 * @param[in,out] last_occurrence Pointer to last occurrence (updated)
 * @return true if null terminator found, false otherwise
 */
static inline bool _process_unaligned_bytes(char **cp, int c, char **last_occurrence) {
    while ((uintptr_t)*cp & (sizeof(uint32_t) - 1)) {
        if (**cp == 0) {
            return true;
        }
        if (**cp == c) {
            *last_occurrence = *cp;
        }
        ++(*cp);
    }
    return false;
}

/**
 * @brief Helper function to process a word with zero byte detection.
 *
 * @param[in] wp Word pointer
 * @param[in] c Character to find
 * @param[in,out] last_occurrence Pointer to last occurrence (updated)
 * @return true if null terminator found, false otherwise
 */
static inline bool _process_word_with_zero(uint32_t *wp, int c, char **last_occurrence) {
    char *cp;
    for (int i = 0; i < (int)sizeof(uint32_t); ++i) {
        cp = ((char *)wp) + i;
        if (*cp == 0) {
            return true;
        }
        if (*cp == c) {
            *last_occurrence = cp;
        }
    }
    return false;
}

/**
 * @brief Helper function to search for character in a word.
 *
 * @param[in] wp Word pointer
 * @param[in] c Character to find
 * @param[in,out] last_occurrence Pointer to last occurrence (updated)
 */
static inline void _search_char_in_word(uint32_t *wp, int c, char **last_occurrence) {
    char *cp;
    for (int i = 0; i < (int)sizeof(uint32_t); ++i) {
        cp = ((char *)wp) + i;
        if (*cp == c) {
            *last_occurrence = cp;
        }
    }
}

/**
 * @brief Helper function to compare two memory blocks of size n.
 *
 * @param[in] s1 First memory block
 * @param[in] s2 Second memory block
 * @param[in] n Number of bytes to compare
 * @return 0 if equal, non-zero otherwise
 */
static inline int _memcmp_n(const void *s1, const void *s2, size_t n) {
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

/**
 * @brief Helper function to check if substring matches at position.
 *
 * @param[in] pos Position to check
 * @param[in] needle Substring to find
 * @param[in] needle_len Length of needle
 * @return true if matches, false otherwise
 */
static inline bool _check_substring_match(const char *pos, const char *needle, size_t needle_len) {
    return _memcmp_n(pos, needle, needle_len) == 0;
}

/**
 * @brief Helper function to search for substring in word-aligned memory.
 *
 * @param[in] wp Word-aligned pointer
 * @param[in] needle Substring to find
 * @param[in] needle_len Length of needle
 * @return Pointer to first occurrence or NULL
 */
static char *_search_in_word(const uint32_t *wp, const char *needle, size_t needle_len) {
    const char *h;

    // Check if we've hit the end of the string in this word
    if (GB_HAS_ZERO(*wp)) {
        // Process each byte in the word individually
        for (int i = 0; i < (int)sizeof(uint32_t); ++i) {
            h = ((const char *)wp) + i;
            if (*h == 0) {
                return NULL;
            }
            if (*h == *needle && _check_substring_match(h, needle, needle_len)) {
                return (char *)h;
            }
        }
        return NULL;
    }

    // Search for the first character of needle in the word
    for (int i = 0; i < (int)sizeof(uint32_t); ++i) {
        h = ((const char *)wp) + i;
        if (*h == *needle && _check_substring_match(h, needle, needle_len)) {
            return (char *)h;
        }
    }
    return NULL;
}

/**
 * @brief Helper function to find substring using word-aligned optimization.
 *
 * @param[in] haystack Pointer to the string to search in
 * @param[in] needle Pointer to the substring to find
 * @param[in] needle_len Length of the needle
 * @return Pointer to first occurrence or NULL
 */
static char *_find_substring_aligned(const char *haystack, const char *needle, size_t needle_len) {
    const char *h = haystack;

    // Process unaligned bytes first
    while ((uintptr_t)h & (sizeof(uint32_t) - 1)) {
        if (*h == 0) {
            return NULL;
        }
        if (*h == *needle && _check_substring_match(h, needle, needle_len)) {
            return (char *)h;
        }
        h++;
    }

    // Process word-aligned bytes
    // Ensure proper alignment by checking if we can safely cast to uint32_t*
    if ((uintptr_t)h % sizeof(uint32_t) == 0) {
// Use compiler pragma to suppress alignment warning since we've verified alignment
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
        const uint32_t *wp = (const uint32_t *)h;
#pragma GCC diagnostic pop

        while (1) {
            char *result = _search_in_word(wp, needle, needle_len);
            if (result != NULL) {
                return result;
            }
            if (GB_HAS_ZERO(*wp)) {
                return NULL;
            }
            wp++;
        }
    }

    // Fallback to byte-by-byte search if not aligned
    while (*h) {
        if (*h == *needle && _check_substring_match(h, needle, needle_len)) {
            return (char *)h;
        }
        h++;
    }

    return NULL;
}

/**
 * @brief Helper function to process unaligned bytes for skipping delimiters.
 *
 * @param[in] current Pointer to current position in string
 * @param[in] is_delim Lookup table for delimiter characters
 * @return Pointer to first non-delimiter character or NULL if end reached
 */
static inline char *_process_unaligned_skip(char *current, const bool *is_delim) {
    while ((uintptr_t)current & (sizeof(uint32_t) - 1)) {
        if (*current == '\0') {
            return NULL;
        }
        if (!is_delim[(unsigned char)*current]) {
            return current;
        }
        ++current;
    }
    return current;
}

/**
 * @brief Helper function to process word-aligned bytes for skipping delimiters.
 *
 * @param[in] current Pointer to current position in string
 * @param[in] is_delim Lookup table for delimiter characters
 * @return Pointer to first non-delimiter character or NULL if end reached
 */
static inline char *_process_aligned_skip(char *current, const bool *is_delim) {
    uint32_t word;
    gb_memcpy(&word, current, sizeof(uint32_t));

    // Check for null terminator
    if (GB_HAS_ZERO(word)) {
        for (int i = 0; i < (int)sizeof(uint32_t); ++i) {
            if (current[i] == '\0') {
                return NULL;
            }
            if (!is_delim[(unsigned char)current[i]]) {
                return current + i;
            }
        }
        return NULL;
    }

    // Check if any byte is not a delimiter
    if (!is_delim[(unsigned char)current[0]] || !is_delim[(unsigned char)current[1]] ||
        !is_delim[(unsigned char)current[2]] || !is_delim[(unsigned char)current[3]]) {
        for (int i = 0; i < (int)sizeof(uint32_t); ++i) {
            if (!is_delim[(unsigned char)current[i]]) {
                return current + i;
            }
        }
    }

    return current + sizeof(uint32_t);
}

/**
 * @brief Helper function to skip delimiter characters using word-aligned optimization.
 *
 * @param[in] current Pointer to current position in string
 * @param[in] is_delim Lookup table for delimiter characters
 * @return Pointer to first non-delimiter character or NULL if end reached
 */
static char *_skip_delimiters(char *current, const bool *is_delim) {
    while (*current) {
        current = _process_unaligned_skip(current, is_delim);
        if (current == NULL || !(*current)) {
            return current;
        }

        if (!((uintptr_t)current & (sizeof(uint32_t) - 1))) {
            char *next_pos = _process_aligned_skip(current, is_delim);
            if (next_pos <= current) {
                return next_pos;
            }
            current = next_pos;
        }
    }
    return NULL;
}

/**
 * @brief Helper function to process unaligned bytes for finding token end.
 *
 * @param[in] current Pointer to current position in string
 * @param[in] is_delim Lookup table for delimiter characters
 * @return Pointer to first delimiter character or end of string
 */
static inline char *_process_unaligned_end(char *current, const bool *is_delim) {
    while ((uintptr_t)current & (sizeof(uint32_t) - 1)) {
        if (*current == '\0' || is_delim[(unsigned char)*current]) {
            return current;
        }
        ++current;
    }
    return current;
}

/**
 * @brief Helper function to process word-aligned bytes for finding token end.
 *
 * @param[in] current Pointer to current position in string
 * @param[in] is_delim Lookup table for delimiter characters
 * @return Pointer to first delimiter character or end of string
 */
static inline char *_process_aligned_end(char *current, const bool *is_delim) {
    uint32_t word;
    gb_memcpy(&word, current, sizeof(uint32_t));

    // Check for null terminator
    if (GB_HAS_ZERO(word)) {
        for (int i = 0; i < (int)sizeof(uint32_t); ++i) {
            if (current[i] == '\0' || is_delim[(unsigned char)current[i]]) {
                return current + i;
            }
        }
        return current + sizeof(uint32_t);
    }

    // Check if any byte is a delimiter
    if (is_delim[(unsigned char)current[0]] || is_delim[(unsigned char)current[1]] ||
        is_delim[(unsigned char)current[2]] || is_delim[(unsigned char)current[3]]) {
        for (int i = 0; i < (int)sizeof(uint32_t); ++i) {
            if (is_delim[(unsigned char)current[i]]) {
                return current + i;
            }
        }
    }

    return current + sizeof(uint32_t);
}

/**
 * @brief Helper function to find end of token using word-aligned optimization.
 *
 * @param[in] current Pointer to current position in string
 * @param[in] is_delim Lookup table for delimiter characters
 * @return Pointer to first delimiter character or end of string
 */
static char *_find_token_end(char *current, const bool *is_delim) {
    while (*current) {
        current = _process_unaligned_end(current, is_delim);
        if (*current == '\0' || is_delim[(unsigned char)*current]) {
            return current;
        }

        if (!((uintptr_t)current & (sizeof(uint32_t) - 1))) {
            char *next_pos = _process_aligned_end(current, is_delim);
            if (next_pos <= current) {
                return next_pos;
            }
            current = next_pos;
        }
    }
    return current;
}

// *****************************************************************************
// *****************************************************************************
// Public Functions
// *****************************************************************************
// *****************************************************************************

/**
 * @brief Copies a block of memory from source to destination.
 *
 * This function copies `len` bytes from the memory area pointed to by `src` to
 * the memory area pointed to by `dst`. It uses Duff's device to optimize the
 * copying of memory.
 *
 * @param[out] dst Pointer to the destination memory area.
 * @param[in] src Pointer to the source memory area.
 * @param[in] len Number of bytes to copy.
 *
 * @return A pointer to the destination memory area `dst`.
 */
void *gb_memcpy(void *dst, const void *src, size_t len) {
    char       *_dst = (char *)dst;
    const char *_src = (const char *)src;

    // For small buffers, use simple loop
    if (len < 8) {
        for (size_t i = 0; i < len; i++) {
            _dst[i] = _src[i];
        }
        return dst;
    }

    // For larger buffers, use optimized approach
    size_t i = 0;

    // Handle initial alignment to 4-byte boundary for both source and destination
    size_t align = (4 - ((uintptr_t)_dst & 3)) & 3;
    if (align > len) {
        align = len;
    }
    for (; i < align; i++) {
        _dst[i] = _src[i];
    }

    // Copy 4 bytes at a time using Duff's device for maximum efficiency
    // Note: Both dst_aligned and src_aligned are guaranteed to be 4-byte aligned
    size_t      len32       = (len - i) / 4;
    char       *dst_aligned = _dst + i;
    const char *src_aligned = _src + i;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

    if (len32 > 0) {
        size_t n = (len32 + 7) / 8; // Number of iterations (8 copies per iteration)
        switch (len32 % 8) {
            case 0:
                do {
                    *(uint32_t *)dst_aligned = *(const uint32_t *)src_aligned;
                    dst_aligned += 4;
                    src_aligned += 4;
                    case 7:
                        *(uint32_t *)dst_aligned = *(const uint32_t *)src_aligned;
                        dst_aligned += 4;
                        src_aligned += 4;
                    case 6:
                        *(uint32_t *)dst_aligned = *(const uint32_t *)src_aligned;
                        dst_aligned += 4;
                        src_aligned += 4;
                    case 5:
                        *(uint32_t *)dst_aligned = *(const uint32_t *)src_aligned;
                        dst_aligned += 4;
                        src_aligned += 4;
                    case 4:
                        *(uint32_t *)dst_aligned = *(const uint32_t *)src_aligned;
                        dst_aligned += 4;
                        src_aligned += 4;
                    case 3:
                        *(uint32_t *)dst_aligned = *(const uint32_t *)src_aligned;
                        dst_aligned += 4;
                        src_aligned += 4;
                    case 2:
                        *(uint32_t *)dst_aligned = *(const uint32_t *)src_aligned;
                        dst_aligned += 4;
                        src_aligned += 4;
                    case 1:
                        *(uint32_t *)dst_aligned = *(const uint32_t *)src_aligned;
                        dst_aligned += 4;
                        src_aligned += 4;
                } while (--n > 0);
            default:
                break;
        }
    }

#pragma GCC diagnostic pop

    // Handle remaining bytes
    i += len32 * 4;
    for (; i < len; i++) {
        _dst[i] = _src[i];
    }

    return dst;
}

/**
 * @brief Sets a block of memory to a specified value.
 *
 * This function sets the first `len` bytes of the memory area pointed to by
 * `dst` to the specified value `val`. It uses Duff's device to optimize the
 * setting of memory.
 *
 * @param[out] dst Pointer to the memory area to be set.
 * @param[in] val The value to be set. The value is passed as an int, but the
 * function fills the memory block using the unsigned char conversion of this
 * value.
 * @param[in] len Number of bytes to be set to the value.
 *
 * @return A pointer to the memory area `dst`.
 */
void *gb_memset(void *dst, int val, size_t len) {
    char *_dst = (char *)dst;

    // For small buffers, use simple loop
    if (len < 8) {
        for (size_t i = 0; i < len; i++) {
            _dst[i] = (char)val;
        }
        return dst;
    }

    // For larger buffers, use optimized approach
    size_t i = 0;

    // Handle initial alignment to 4-byte boundary
    size_t align = (4 - ((uintptr_t)_dst & 3)) & 3;
    if (align > len) {
        align = len;
    }
    for (; i < align; i++) {
        _dst[i] = (char)val;
    }

    // Set 4 bytes at a time using Duff's device for maximum efficiency
    // Note: dst_aligned is guaranteed to be 4-byte aligned by the alignment loop above
    uint32_t val32       = (uint32_t)(unsigned char)val * 0x01010101UL;
    size_t   len32       = (len - i) / 4;
    char    *dst_aligned = _dst + i;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

    if (len32 > 0) {
        size_t n = (len32 + 7) / 8; // Number of iterations (8 copies per iteration)
        switch (len32 % 8) {
            case 0:
                do {
                    *(uint32_t *)dst_aligned = val32;
                    dst_aligned += 4;
                    case 7:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 6:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 5:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 4:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 3:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 2:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 1:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                } while (--n > 0);
            default:
                break;
        }
    }

#pragma GCC diagnostic pop

    // Handle remaining bytes
    i += len32 * 4;
    for (; i < len; i++) {
        _dst[i] = (char)val;
    }

    return dst;
}

/**
 * @brief Sets a block of memory to zero.
 *
 * This function sets the memory block pointed to by `dst` to zero, for a total
 * of `len` bytes. It uses a loop unrolling technique to optimize the process.
 *
 * @param[out] dst Pointer to the memory block to be zeroed.
 * @param[in] len Number of bytes to set to zero.
 */
void gb_bzero(void *dst, size_t len) {
    char *_dst = (char *)dst;

    // For small buffers, use simple loop
    if (len < 8) {
        for (size_t i = 0; i < len; i++) {
            _dst[i] = 0;
        }
        return;
    }

    // For larger buffers, use optimized approach
    size_t i = 0;

    // Handle initial alignment to 4-byte boundary
    size_t align = (4 - ((uintptr_t)_dst & 3)) & 3;
    if (align > len) {
        align = len;
    }
    for (; i < align; i++) {
        _dst[i] = 0;
    }

    // Set 4 bytes at a time using Duff's device for maximum efficiency
    // Note: dst_aligned is guaranteed to be 4-byte aligned by the alignment loop above
    uint32_t val32       = 0;
    size_t   len32       = (len - i) / 4;
    char    *dst_aligned = _dst + i;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

    if (len32 > 0) {
        size_t n = (len32 + 7) / 8; // Number of iterations (8 copies per iteration)
        switch (len32 % 8) {
            case 0:
                do {
                    *(uint32_t *)dst_aligned = val32;
                    dst_aligned += 4;
                    case 7:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 6:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 5:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 4:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 3:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 2:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                    case 1:
                        *(uint32_t *)dst_aligned = val32;
                        dst_aligned += 4;
                } while (--n > 0);
            default:
                break;
        }
    }

#pragma GCC diagnostic pop

    // Handle remaining bytes
    i += len32 * 4;
    for (; i < len; i++) {
        _dst[i] = 0;
    }
}

/**
 * @brief Finds the first occurrence of a character in a string.
 *
 * This function searches for the first occurrence of the character `c` in the
 * string `str`. If `c` is the null character '\0', it returns a pointer to the
 * terminating null character.
 *
 * @param[in] str Pointer to the null-terminated string to search.
 * @param[in] c The character to locate (passed as an int).
 *
 * @return A pointer to the first occurrence of `c` in `str`, or `NULL` if the
 * character is not found.
 */
char *gb_strchr(const char *str, int c) {
    if (!str) {
        return NULL;
    }

    char *cp = (char *)str; // NOSONAR (drop const qualifier)

    if (c == 0) {
        // clang-format off
        while (*cp) { ++cp; }
        // clang-format on
        return cp;
    }

    while ((uintptr_t)cp & (sizeof(uint32_t) - 1)) {
        // clang-format off
        if (*cp == 0) { return NULL; }
        if (*cp == c) { return cp;   }
        // clang-format on
        ++cp;
    }

    void     *vp = (void *)cp;
    uint32_t *wp = (uint32_t *)vp;

    while (1) {
        for (int i = 0; i < (int)sizeof(uint32_t); ++i) {
            cp = ((char *)wp) + i;
            // clang-format off
            if (*cp == 0) { return NULL; }
            if (*cp == c) { return cp;   }
            // clang-format on
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
 * @param[in] str Pointer to the null-terminated string to search.
 * @param[in] c The character to locate (passed as an int).
 *
 * @return A pointer to the last occurrence of `c` in `str`, or `NULL` if the
 * character is not found.
 */
char *gb_strrchr(const char *str, int c) {
    if (!str) {
        return NULL;
    }

    char *last_occurrence = NULL;
    char *cp              = (char *)str; // NOSONAR (drop const qualifier)

    if (c == 0) {
        // Find the null terminator
        // clang-format off
        while (*cp) { ++cp; }
        // clang-format on
        return cp;
    }

    // Process unaligned bytes first
    if (_process_unaligned_bytes(&cp, c, &last_occurrence)) {
        return last_occurrence;
    }

    // Process word-aligned bytes for performance
    void     *vp = (void *)cp;
    uint32_t *wp = (uint32_t *)vp;

    while (1) {
        // Check if we've hit the end of the string in this word
        if (GB_HAS_ZERO(*wp)) {
            if (_process_word_with_zero(wp, c, &last_occurrence)) {
                return last_occurrence;
            }
            return last_occurrence;
        }

        // Search for the character in the word
        _search_char_in_word(wp, c, &last_occurrence);
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
 * @param[in] haystack Pointer to the null-terminated string to search in.
 * @param[in] needle Pointer to the null-terminated substring to find.
 *
 * @return A pointer to the first occurrence of `needle` in `haystack`, or
 * `NULL` if the substring is not found.
 */
char *gb_strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) {
        return NULL;
    }

    // Empty needle matches the beginning of haystack
    if (*needle == 0) {
        return (char *)haystack;
    }

    // Empty haystack cannot contain non-empty needle
    if (*haystack == 0) {
        return NULL;
    }

    // Use optimized length calculation
    size_t needle_len   = gb_strlen(needle);
    size_t haystack_len = gb_strlen(haystack);

    // If needle is longer than haystack, it cannot be found
    if (needle_len > haystack_len) {
        return NULL;
    }

    // Use optimized search for single character needle
    if (needle_len == 1) {
        return gb_strchr(haystack, *needle);
    }

    // Use word-aligned search for longer needles
    return _find_substring_aligned(haystack, needle, needle_len);
}

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
 * @param[out] dst Pointer to the destination buffer.
 * @param[in] src Pointer to the source string.
 *
 * @return A pointer to the destination buffer.
 */
char *gb_strcpy(char *dst, const char *src) {
    if (!dst || !src) {
        return dst;
    }

    char *ptr = dst;

    while ((*src) && (((uintptr_t)ptr % sizeof(uint32_t)) || ((uintptr_t)src % sizeof(uint32_t)))) {
        *ptr = *src;

        ++ptr;
        ++src;
    }

    void       *vp1 = (void *)ptr;
    const void *vp2 = (const void *)src;

    uint32_t       *wp1 = (uint32_t *)vp1;
    const uint32_t *wp2 = (const uint32_t *)vp2;

    while (!GB_HAS_ZERO(*wp1) && !GB_HAS_ZERO(*wp2)) {
        *wp1 = *wp2;

        ++wp1;
        ++wp2;
    }

    ptr = (char *)wp1;
    src = (const char *)wp2;

    while (*src) {
        *ptr = *src;

        ++ptr;
        ++src;
    }

    *ptr = '\0';

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
 * @param[out] dst Pointer to the destination buffer.
 * @param[in] src Pointer to the source string.
 * @param[in] n The maximum number of characters to copy from `src`.
 *
 * @return A pointer to the destination buffer.
 */
char *gb_strncpy(char *dst, const char *src, size_t n) {
    if (!dst || !src || !n) {
        return dst;
    }

    char *ptr = dst;

    while ((n > 0) && (*src) &&                    //
           (((uintptr_t)ptr % sizeof(uint32_t)) || //
            ((uintptr_t)src % sizeof(uint32_t)))) {
        *ptr = *src;

        ++ptr;
        ++src;
        --n;
    }

    void       *vp1 = (void *)ptr;
    const void *vp2 = (const void *)src;

    uint32_t       *wp1 = (uint32_t *)vp1;
    const uint32_t *wp2 = (const uint32_t *)vp2;

    while ((n >= sizeof(uint32_t)) && !GB_HAS_ZERO(*wp1) && !GB_HAS_ZERO(*wp2)) {
        *wp1 = *wp2;

        ++wp1;
        ++wp2;
        n -= sizeof(uint32_t);
    }

    ptr = (char *)wp1;
    src = (const char *)wp2;

    while (*src) {
        *ptr = *src;

        ++ptr;
        ++src;
    }

    *ptr = '\0';

    return dst;
}

/**
 * @brief Compares two strings lexicographically.
 *
 * This function compares the two null-terminated strings `str1` and `str2`.
 *
 * @param[in] str1 Pointer to the first string.
 * @param[in] str2 Pointer to the second string.
 *
 * @return An integer less than, equal to, or greater than zero if `str1` is
 * found, respectively, to be less than, to match, or be greater than `str2`.
 */
int gb_strcmp(const char *str1, const char *str2) {
    if (!str1 || !str2) {
        int diff = str1 ? 1 : -1;
        return (str1 == str2) ? 0 : diff;
    }

    while (((uintptr_t)str1 % sizeof(uint32_t)) || //
           ((uintptr_t)str2 % sizeof(uint32_t))) {
        if ((*str1 != *str2) || !(*str1) || !(*str2)) {
            return (unsigned char)*str1 - (unsigned char)*str2;
        }

        ++str1;
        ++str2;
    }

    const void *vp1 = (const void *)str1;
    const void *vp2 = (const void *)str2;

    const uint32_t *wp1 = (const uint32_t *)vp1;
    const uint32_t *wp2 = (const uint32_t *)vp2;

    while (!GB_HAS_ZERO(*wp1) && !GB_HAS_ZERO(*wp2) && (*wp1 == *wp2)) {
        ++wp1;
        ++wp2;
    }

    if (*wp1 == *wp2) {
        return 0;
    }

    str1 = (const char *)wp1;
    str2 = (const char *)wp2;

    while (*str1 && *str2 && (*str1 == *str2)) {
        ++str1;
        ++str2;
    }

    return ((unsigned char)*str1 - (unsigned char)*str2);
}

/**
 * @brief Compares up to `n` characters of two strings lexicographically.
 *
 * This function compares up to `n` characters of the two null-terminated
 * strings `str1` and `str2`.
 *
 * @param[in] str1 Pointer to the first string.
 * @param[in] str2 Pointer to the second string.
 * @param[in] n The maximum number of characters to compare.
 *
 * @return An integer less than, equal to, or greater than zero if the first `n`
 * bytes of `str1` is found, respectively, to be less than, to match, or be
 * greater than the first `n` bytes of `str2`.
 */
int gb_strncmp(const char *str1, const char *str2, size_t n) {
    if (n == 0) {
        return 0;
    }

    if (!str1 || !str2) {
        int diff = str1 ? 1 : -1;
        return (str1 == str2) ? 0 : diff;
    }

    while ((n > 0) && (((uintptr_t)str1 % sizeof(uint32_t)) || //
                       ((uintptr_t)str2 % sizeof(uint32_t)))) {
        if ((*str1 != *str2) || !(*str1) || !(*str2)) {
            return (unsigned char)*str1 - (unsigned char)*str2;
        }

        ++str1;
        ++str2;
        --n;
    }

    const void *vp1 = (const void *)str1;
    const void *vp2 = (const void *)str2;

    const uint32_t *wp1 = (const uint32_t *)vp1;
    const uint32_t *wp2 = (const uint32_t *)vp2;

    while ((n >= sizeof(uint32_t)) && !GB_HAS_ZERO(*wp1) && !GB_HAS_ZERO(*wp2) && (*wp1 == *wp2)) {
        ++wp1;
        ++wp2;
        n -= sizeof(uint32_t);
    }

    if (*wp1 == *wp2) {
        return 0;
    }

    str1 = (const char *)wp1;
    str2 = (const char *)wp2;

    while ((n > 0) && *str1 && *str2 && (*str1 == *str2)) {
        ++str1;
        ++str2;
        --n;
    }

    if (n == 0) {
        return 0;
    }

    return ((unsigned char)*str1 - (unsigned char)*str2);
}

/**
 * @brief Finds the first occurrence of any character from a set of characters.
 *
 * This function scans the null-terminated string `str` for the first
 * occurrence of any character from the null-terminated string `reject`.
 *
 * @param[in] str Pointer to the null-terminated string to search.
 * @param[in] reject Pointer to the null-terminated string containing characters to reject.
 *
 * @return The number of characters at the beginning of `str` that are not
 * in the `reject` string.
 */
size_t gb_strcspn(const char *str, const char *reject) {
    if (!str || !reject) {
        return 0;
    }

    // Create lookup table for O(1) character checking
    static bool lookup[256];
    for (int i = 0; i < 256; ++i) {
        lookup[i] = false;
    }

    const char *r = reject;
    while (*r) {
        lookup[(unsigned char)*r] = true;
        ++r;
    }

    const char *p = str;

    // Simple optimized loop with lookup table
    while (*p) {
        if (lookup[(unsigned char)*p]) {
            break;
        }
        ++p;
    }

    return (size_t)(p - str);
}

/*=============================================================================
  PUBLIC FUNCTIONS
=============================================================================*/

/**
 * @brief Splits a string into tokens using delimiters.
 *
 * This function parses the string `str` into a sequence of tokens, separated
 * by one or more characters from the string `delim`. The `saveptr` argument
 * maintains state between calls to continue tokenization of the same string.
 *
 * This is a thread-safe implementation equivalent to the standard strtok_r.
 *
 * @param[in] str String to tokenize (NULL to continue with previous string)
 * @param[in] delim String containing delimiter characters
 * @param[in,out] saveptr Pointer to save parsing state between calls
 *
 * @return Pointer to the next token, or NULL if no more tokens exist.
 */
char *gb_strtok_r(char *str, const char *delim, char **saveptr) {
    if (!delim || !saveptr) {
        return NULL;
    }

    // If str is not NULL, start new tokenization
    if (str != NULL) {
        *saveptr = str;
    }

    char *current = *saveptr;
    if (current == NULL) {
        return NULL;
    }

    // Persistent lookup table for delimiter characters
    static bool is_delim[256];
    static char last_delims[32]   = {0};
    static bool table_initialized = false;

    // Only rebuild table if delimiters changed
    if (!table_initialized || gb_strcmp(last_delims, delim) != 0) {
        // Initialize lookup table
        for (int i = 0; i < 256; ++i) {
            is_delim[i] = false;
        }

        const char *d = delim;
        while (*d) {
            is_delim[(unsigned char)*d] = true;
            ++d;
        }

        // Save current delimiters for next comparison
        gb_strncpy(last_delims, delim, sizeof(last_delims) - 1);
        last_delims[sizeof(last_delims) - 1] = '\0';
        table_initialized                    = true;
    }

    // Skip leading delimiters
    current = _skip_delimiters(current, is_delim);
    if (current == NULL) {
        *saveptr = NULL;
        return NULL;
    }

    // Find end of token
    char *token_start = current;
    char *token_end   = _find_token_end(current, is_delim);

    // Null-terminate the token
    if (*token_end != '\0') {
        *token_end = '\0';
        ++token_end;
    }

    *saveptr = token_end;
    return token_start;
}

/**
 * @brief Calculates the length of a string.
 *
 * This function calculates the length of the null-terminated string `str`.
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

    while ((uintptr_t)cp & (sizeof(uint32_t) - 1)) {
        // clang-format off
        if (!*cp) { return (size_t)(cp - str); }
        // clang-format on
        ++cp;
    }

    const void     *vs = (const void *)str;
    const void     *vp = (const void *)cp;
    const uint32_t *wp = (const uint32_t *)vp;

    while (1) {
        if (GB_HAS_ZERO(*wp)) {
            // clang-format off
            if (!(*wp & 0x000000FF)) { return ((uintptr_t)wp + 0 - ((uintptr_t)vs)); }
            if (!(*wp & 0x0000FF00)) { return ((uintptr_t)wp + 1 - ((uintptr_t)vs)); }
            if (!(*wp & 0x00FF0000)) { return ((uintptr_t)wp + 2 - ((uintptr_t)vs)); }
            if (!(*wp & 0xFF000000)) { return ((uintptr_t)wp + 3 - ((uintptr_t)vs)); }
            // clang-format on
        }
        ++wp;
    }
}

/**
 * @brief Converts a binary string to a decimal string.
 *
 * This function converts a binary number represented as a string `src_bin` into
 * a decimal string `dst_dec`.
 *
 * @param[in] src_bin Pointer to the source binary string.
 * @param[out] dst_dec Pointer to the destination decimal string.
 * @param[in] dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_bin2dec(const char *src_bin, char *dst_dec, size_t dst_len) {
    bool rvalue = (src_bin && dst_dec && dst_len);

    if (rvalue) {
        char  *err;
        size_t num = strtoul(src_bin, &err, 2);

        rvalue = !(*err);

        if (rvalue) {
            rvalue = snprintf(dst_dec, dst_len, "%zd", num) > 0;
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
 * @param[in] src_bin Pointer to the source binary string.
 * @param[out] dst_hex Pointer to the destination hexadecimal string.
 * @param[in] dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_bin2hex(const char *src_bin, char *dst_hex, size_t dst_len) {
    bool rvalue = (src_bin && dst_hex && dst_len);

    if (rvalue) {
        char  *err;
        size_t num = strtoul(src_bin, &err, 2);

        rvalue = !(*err);

        if (rvalue) {
            rvalue = snprintf(dst_hex, dst_len, "%zX", num) > 0;
        }
    }

    return rvalue;
}

/**
 * @brief Converts a decimal string to a binary string.
 *
 * @param[in] num Decimal number to convert
 * @param[out] dst_bin Pointer to the destination binary string.
 * @param[in] dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool _unsafe_dec2bin(size_t num, char *dst_bin, size_t dst_len) {
#if SIZE_MAX == 0xFFFFFFFFUL
    int bits = (num) ? 32 - __builtin_clz(num) : 1;
#else
    int bits = num ? 64 - __builtin_clzl(num) : 1;
#endif

    int rem = bits % 8;
    int pad = (rem > 0) ? (8 - rem) : 0;

    if ((pad + bits) < (int)dst_len) {
        for (int i = 0; i < pad; ++i) {
            dst_bin[i] = '0';
        }

        for (int i = 0; i < bits; ++i) {
            dst_bin[pad + bits - 1 - i] = (char)((num & (1UL << i)) ? '1' : '0');
        }

        dst_bin[pad + bits] = '\0';

        return true;
    }

    return false;
}

/**
 * @brief Converts a decimal string to a binary string.
 *
 * This function converts a decimal number represented as a string `src_dec`
 * into a binary string `dst_bin`. The output is padded with leading zeros to
 * ensure it is a multiple of 8 characters.
 *
 * @param[in] src_dec Pointer to the source decimal string.
 * @param[out] dst_bin Pointer to the destination binary string.
 * @param[in] dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_dec2bin(const char *src_dec, char *dst_bin, size_t dst_len) {
    bool rvalue = (src_dec && dst_bin && dst_len);

    if (rvalue) {
        char  *err;
        size_t num = strtoul(src_dec, &err, 10);

        rvalue = !(*err);

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
 * @param[in] src_dec Pointer to the source decimal string.
 * @param[out] dst_hex Pointer to the destination hexadecimal string.
 * @param[in] dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_dec2hex(const char *src_dec, char *dst_hex, size_t dst_len) {
    bool rvalue = (src_dec && dst_hex && dst_len);

    if (!rvalue) {
        return false;
    }

    char  *err;
    size_t num = strtoul(src_dec, &err, 10);

    rvalue = !(*err);

    if (rvalue) {
        rvalue = snprintf(dst_hex, dst_len, "%zX", num) > 0;
    }

    if (rvalue) {
        int len = (int)gb_strlen(dst_hex);
        int rem = len % 4;
        int pad = 4 - rem;

        if (rem > 0 && ((pad + len) < (int)dst_len)) {
            for (int i = len; i >= 0; --i) {
                dst_hex[i + pad] = dst_hex[i];
            }

            for (int i = 0; i < pad; ++i) {
                dst_hex[i] = '0';
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
 * @param[in] src_hex Pointer to the source hexadecimal string.
 * @param[out] dst_bin Pointer to the destination binary string.
 * @param[in] dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2bin(const char *src_hex, char *dst_bin, size_t dst_len) {
    bool rvalue = (src_hex && dst_bin && dst_len);

    if (rvalue) {
        char  *err;
        size_t num = strtoul(src_hex, &err, 16);

        rvalue = !(*err);

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
 * @param[in] src_hex Pointer to the source hexadecimal string.
 * @param[out] dst_dec Pointer to the destination decimal string.
 * @param[in] dst_len Length of the destination buffer.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2dec(const char *src_hex, char *dst_dec, size_t dst_len) {
    bool rvalue = (src_hex && dst_dec && dst_len);

    if (rvalue) {
        char  *err;
        size_t num = strtoul(src_hex, &err, 16);

        rvalue = !(*err);

        if (rvalue) {
            rvalue = snprintf(dst_dec, dst_len, "%zd", num) > 0;
        }
    }

    return rvalue;
}

/**
 * @brief Converts a binary data buffer to its hexadecimal string
 * representation.
 *
 * This function takes a buffer of raw bytes and converts it into a
 * null-terminated string of hexadecimal characters. Each byte in the source
 * buffer is represented by two characters in the destination string.
 *
 * @param[in] src_hex Pointer to the source buffer containing the binary data.
 * @param[in] src_len The number of bytes in the source buffer to convert.
 * @param[out] dst_str Pointer to the destination buffer where the resulting
 * hexadecimal string will be stored.
 * @param[in] dst_len The size of the destination buffer in bytes. This must be
 * at least (src_len * 2) + 1 to accommodate the full hexadecimal string and the
 * null terminator.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2str(const char *src_hex, size_t src_len, char *dst_str, size_t dst_len) {
    bool rvalue = (src_hex && src_len && dst_str && dst_len > src_len * 2);

    if (rvalue) {
        const char hex_chars[] = "0123456789ABCDEF";

        for (size_t i = 0; i < src_len; ++i) {
            dst_str[(i * 2) + 0] = hex_chars[(src_hex[i] >> 4) & 0x0F];
            dst_str[(i * 2) + 1] = hex_chars[(src_hex[i] >> 0) & 0x0F];
        }

        dst_str[src_len * 2] = '\0';
    }

    return rvalue;
}

/**
 * @brief Converts a binary data buffer to its hexadecimal string
 * representation, with byte reversal.
 *
 * This function reads a hexadecimal string and converts it into a raw byte
 * buffer. It processes the input string from right to left, two characters (one
 * byte) at a time. This results in a byte-reversed (endian-swapped) output
 * buffer.
 *
 * For example, the input string "1A2B3C" would be processed as "3C", then "2B",
 * then "1A", resulting in the output byte buffer {0x3C, 0x2B, 0x1A}.
 *
 * @param[in] src_str Pointer to the null-terminated source string of
 * hexadecimal characters. The length of this string must be an even number.
 * @param[out] dst_hex Pointer to the destination buffer where the resulting
 * binary data will be stored.
 * @param[in] dst_len The size of the destination buffer in bytes. This must be
 * at least half the length of the source string.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2str_r(const char *src_hex, size_t src_len, char *dst_str, size_t dst_len) {
    bool rvalue = (src_hex && src_len && dst_str && dst_len > src_len * 2);

    if (rvalue) {
        const char hex_chars[] = "0123456789ABCDEF";

        for (size_t i = 0; i < src_len; ++i) {
            dst_str[(i * 2) + 0] = hex_chars[(src_hex[src_len - 1 - i] >> 4) & 0x0F];
            dst_str[(i * 2) + 1] = hex_chars[(src_hex[src_len - 1 - i] >> 0) & 0x0F];
        }

        dst_str[src_len * 2] = '\0';
    }

    return rvalue;
}

/* *****************************************************************************
 End of File
 */
