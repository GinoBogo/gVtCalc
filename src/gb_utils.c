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

#include <stdio.h>  // snprintf
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
// Public Functions
// *****************************************************************************
// *****************************************************************************

// NOTE: not all compilers support the __attribute__((fallthrough))
#define FALL_THROUGH __attribute__((fallthrough))

/**
 * @brief Duff's Device for loop unrolling without leftover extra code.
 *
 * This macro implements Duff's Device, a technique for loop unrolling that
 * reduces the overhead of loop control by interleaving the loop control code
 * with the loop body. It is used to copy a value `x` to the destination array
 * `_dst` multiple times.
 *
 * @param x The value to be copied to the destination array.
 */
#define DUFF_DEVICE(x) \
    case 7:            \
        _dst[i] = x;   \
        ++i;           \
        FALL_THROUGH;  \
    case 6:            \
        _dst[i] = x;   \
        ++i;           \
        FALL_THROUGH;  \
    case 5:            \
        _dst[i] = x;   \
        ++i;           \
        FALL_THROUGH;  \
    case 4:            \
        _dst[i] = x;   \
        ++i;           \
        FALL_THROUGH;  \
    case 3:            \
        _dst[i] = x;   \
        ++i;           \
        FALL_THROUGH;  \
    case 2:            \
        _dst[i] = x;   \
        ++i;           \
        FALL_THROUGH;  \
    case 1:            \
        _dst[i] = x;   \
        ++i;

/**
 * @brief Copies a block of memory from source to destination.
 *
 * This function copies `len` bytes from the memory area pointed to by `src` to
 * the memory area pointed to by `dst`. It uses Duff's device to optimize the
 * copying of memory.
 *
 * @param dst Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param len Number of bytes to copy.
 */
void gb_memcpy(void *dst, void *src, size_t len) {
    char *_dst = (char *)dst;
    char *_src = (char *)src;

    size_t i   = 0;
    size_t div = len / 8;
    size_t rem = len % 8;

    switch (rem) {
        default:
            break;

        case 0:
            while (!(div == 0)) {
                --div;
                _dst[i] = _src[i];
                ++i;

                FALL_THROUGH;
                DUFF_DEVICE(_src[i]);
            };
    }
}

/**
 * @brief Sets a block of memory to a specified value.
 *
 * This function sets the first `len` bytes of the memory area pointed to by
 * `dst` to the specified value `val`. It uses Duff's device to optimize the
 * setting of memory.
 *
 * @param dst Pointer to the memory area to be set.
 * @param val The value to be set. Only the lower 8 bits of `val` are used.
 * @param len Number of bytes to be set to the value.
 */
void gb_memset(void *dst, char val, size_t len) {
    char *_dst = (char *)dst;

    size_t i   = 0;
    size_t div = len / 8;
    size_t rem = len % 8;

    switch (rem) {
        default:
            break;

        case 0:
            while (!(div == 0)) {
                --div;
                _dst[i] = val;
                ++i;

                FALL_THROUGH;
                DUFF_DEVICE(val);
            };
    }
}

/**
 * @brief Sets a block of memory to zero.
 *
 * This function sets the memory block pointed to by `dst` to zero, for a total
 * of `len` bytes. It uses a loop unrolling technique to optimize the process.
 *
 * @param dst Pointer to the memory block to be zeroed.
 * @param len Number of bytes to set to zero.
 */
void gb_zeros(void *dst, size_t len) {
    char *_dst = (char *)dst;

    size_t i   = 0;
    size_t div = len / 8;
    size_t rem = len % 8;

    switch (rem) {
        default:
            break;

        case 0:
            while (!(div == 0)) {
                --div;
                _dst[i] = 0;
                ++i;

                FALL_THROUGH;
                DUFF_DEVICE(0);
            };
    }
}

/**
 * @brief Finds the first occurrence of a character in a string.
 *
 * This function searches for the first occurrence of the character `c` in the
 * string `str`. If found, it returns a pointer to that character in the string.
 * If not found, it returns NULL.
 *
 * @param str Pointer to the string.
 * @param c The character to search for.
 * @return Pointer to the first occurrence of `c` in `str`, or NULL if not
 * found.
 */
char *gb_strchr(const char *str, int c) {
    if (!str) {
        return NULL;
    }

    char *cp = (char *)str;

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
 * @brief Compares two strings for equality.
 *
 * This function compares two null-terminated strings `str1` and `str2` for
 * equality. It returns 0 if the strings are equal, a negative value if `str1`
 * is less than `str2`, and a positive value if `str1` is greater than `str2`.
 *
 * @param str1 Pointer to the first string.
 * @param str2 Pointer to the second string.
 * @return 0 if equal, negative if `str1` < `str2`, positive if `str1` > `str2`.
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

    void *vp1 = (void *)str1;
    void *vp2 = (void *)str2;

    uint32_t *wp1 = (uint32_t *)vp1;
    uint32_t *wp2 = (uint32_t *)vp2;

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

    return (int)((unsigned char)*str1 - (unsigned char)*str2);
}

/**
 * @brief Compares up to `n` characters of two strings.
 *
 * This function compares up to `n` characters of the two null-terminated
 * strings `str1` and `str2`. It returns 0 if the compared parts of the strings
 * are equal, a negative value if `str1` is less than `str2`, and a positive
 * value if `str1` is greater than `str2`.
 *
 * @param str1 Pointer to the first string.
 * @param str2 Pointer to the second string.
 * @param n Maximum number of characters to compare.
 * @return 0 if equal, negative if `str1` < `str2`, positive if `str1` > `str2`.
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

    void *vp1 = (void *)str1;
    void *vp2 = (void *)str2;

    uint32_t *wp1 = (uint32_t *)vp1;
    uint32_t *wp2 = (uint32_t *)vp2;

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

    return (int)((unsigned char)*str1 - (unsigned char)*str2);
}

/**
 * @brief Returns the length of a string.
 *
 * This function calculates the length of a null-terminated string `str`.
 *
 * @param str Pointer to the string.
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

    void     *vs = (void *)str;
    void     *vp = (void *)cp;
    uint32_t *wp = (uint32_t *)vp;

    while (1) {
        if (GB_HAS_ZERO(*wp)) {
            // clang-format off
            if (!(*wp & 0x000000FF)) { return (size_t)((uintptr_t)wp + 0 - ((uintptr_t)vs)); }
            if (!(*wp & 0x0000FF00)) { return (size_t)((uintptr_t)wp + 1 - ((uintptr_t)vs)); }
            if (!(*wp & 0x00FF0000)) { return (size_t)((uintptr_t)wp + 2 - ((uintptr_t)vs)); }
            if (!(*wp & 0xFF000000)) { return (size_t)((uintptr_t)wp + 3 - ((uintptr_t)vs)); }
            // clang-format on
        }
        ++wp;
    }
}

/**
 * @brief Converts a binary string to a decimal string.
 *
 * This function converts a binary number represented as a string `src_bin` into
 * a decimal string `dst_dec`. The output is padded with leading zeros to ensure
 * it is a multiple of 4 characters.
 *
 * @param src_bin Pointer to the source binary string.
 * @param dst_dec Pointer to the destination decimal string.
 * @param dst_len Length of the destination buffer.
 * @return true if the conversion was successful, false otherwise.
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
 * a hexadecimal string `dst_hex`. The output is padded with leading zeros to
 * ensure it is a multiple of 4 characters.
 *
 * @param src_bin Pointer to the source binary string.
 * @param dst_hex Pointer to the destination hexadecimal string.
 * @param dst_len Length of the destination buffer.
 * @return true if the conversion was successful, false otherwise.
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

bool __unsafe_dec2bin(size_t num, char *dst_bin, size_t dst_len) {
#if SIZE_MAX == 0xFFFFFFFFUL
    int bits = (num) ? 32 - __builtin_clz(num) : 1;
#else
    int bits = (num) ? 64 - __builtin_clzl(num) : 1;
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
 * @param src_dec Pointer to the source decimal string.
 * @param dst_bin Pointer to the destination binary string.
 * @param dst_len Length of the destination buffer.
 * @return true if the conversion was successful, false otherwise.
 */
bool gb_dec2bin(const char *src_dec, char *dst_bin, size_t dst_len) {
    bool rvalue = (src_dec && dst_bin && dst_len);

    if (rvalue) {
        char  *err;
        size_t num = strtoul(src_dec, &err, 10);

        rvalue = !(*err);

        if (rvalue) {
            rvalue = __unsafe_dec2bin(num, dst_bin, dst_len);
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
 * @param src_dec Pointer to the source decimal string.
 * @param dst_hex Pointer to the destination hexadecimal string.
 * @param dst_len Length of the destination buffer.
 * @return true if the conversion was successful, false otherwise.
 */
bool gb_dec2hex(const char *src_dec, char *dst_hex, size_t dst_len) {
    bool rvalue = (src_dec && dst_hex && dst_len);

    if (rvalue) {
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
 * @param src_hex Pointer to the source hexadecimal string.
 * @param dst_bin Pointer to the destination binary string.
 * @param dst_len Length of the destination buffer.
 * @return true if the conversion was successful, false otherwise.
 */
bool gb_hex2bin(const char *src_hex, char *dst_bin, size_t dst_len) {
    bool rvalue = (src_hex && dst_bin && dst_len);

    if (rvalue) {
        char  *err;
        size_t num = strtoul(src_hex, &err, 16);

        rvalue = !(*err);

        if (rvalue) {
            rvalue = __unsafe_dec2bin(num, dst_bin, dst_len);
        }
    }

    return rvalue;
}

/**
 * @brief Converts a hexadecimal string to a decimal string.
 *
 * This function converts a hexadecimal number represented as a string `src_hex`
 * into a decimal string `dst_dec`. The output is padded with leading zeros to
 * ensure it is a multiple of 4 characters.
 *
 * @param src_hex Pointer to the source hexadecimal string.
 * @param dst_dec Pointer to the destination decimal string.
 * @param dst_len Length of the destination buffer.
 * @return true if the conversion was successful, false otherwise.
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

/* *****************************************************************************
 End of File
 */
