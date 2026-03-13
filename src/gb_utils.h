/* ************************************************************************** */
/*
    @file
        gb_utils.h

    @date
        October, 2023

    @author
        Gino Francesco Bogo (ᛊᛟᚱᚱᛖ ᛗᛖᚨ ᛁᛊᛏᚨᛗᛁ ᚨcᚢᚱᛉᚢ)

    @license
        MIT
 */
/* ************************************************************************** */

#ifndef GB_UTILS_H
#define GB_UTILS_H

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t, uint16_t, uint32_t

// *****************************************************************************
// *****************************************************************************
// Public Macros
// *****************************************************************************
// *****************************************************************************

// RAM MEMORY macros
#define MEM_ALIGNED __attribute__((aligned(32)))
#define PCK_ALIGNED __attribute__((packed, aligned(4)))
#define RAM_NOCACHE __attribute__((space(data), section(".ram_nocache")))

// OPTIMIZE macros
#define OPTIMIZE_CODE __attribute__((optimize("O2")))
#define OPTIMIZE_SIZE __attribute__((optimize("Os")))
#define OPTIMIZE_PURE __attribute__((pure))
#define OPTIMIZE_RAM  __attribute__((ramfunc))

// MASK-HELPER macros
#define SHIFT_U8(b, s) ((uint8_t)(b) << (s))

#define MASK_FROM_2_BOOL(b2, b1)     (SHIFT_U8((b1), 0) | SHIFT_U8((b2), 1))
#define MASK_FROM_3_BOOL(b3, b2, b1) (SHIFT_U8((b1), 0) | SHIFT_U8((b2), 1) | SHIFT_U8((b3), 2))
#define MASK_FROM_4_BOOL(b4, b3, b2, b1) \
    (SHIFT_U8((b1), 0) | SHIFT_U8((b2), 1) | SHIFT_U8((b3), 2) | SHIFT_U8((b4), 3))

#undef SHIFT_U8

// NIBBLE macros
#define N_0000 0
#define N_0001 1
#define N_0010 2
#define N_0011 3
#define N_0100 4
#define N_0101 5
#define N_0110 6
#define N_0111 7
#define N_1000 8
#define N_1001 9
#define N_1010 A
#define N_1011 B
#define N_1100 C
#define N_1101 D
#define N_1110 E
#define N_1111 F

// BYTE macros
#define B_00000000 00
#define B_00000001 01
#define B_00000010 02
#define B_00000011 03
#define B_00000100 04
#define B_00000101 05
#define B_00000110 06
#define B_00000111 07
#define B_00001000 08
#define B_00001001 09
#define B_00001010 0A
#define B_00001011 0B
#define B_00001100 0C
#define B_00001101 0D
#define B_00001110 0E
#define B_00001111 0F
#define B_00010000 10
#define B_00010001 11
#define B_00010010 12
#define B_00010011 13
#define B_00010100 14
#define B_00010101 15
#define B_00010110 16
#define B_00010111 17
#define B_00011000 18
#define B_00011001 19
#define B_00011010 1A
#define B_00011011 1B
#define B_00011100 1C
#define B_00011101 1D
#define B_00011110 1E
#define B_00011111 1F
#define B_00100000 20
#define B_00100001 21
#define B_00100010 22
#define B_00100011 23
#define B_00100100 24
#define B_00100101 25
#define B_00100110 26
#define B_00100111 27
#define B_00101000 28
#define B_00101001 29
#define B_00101010 2A
#define B_00101011 2B
#define B_00101100 2C
#define B_00101101 2D
#define B_00101110 2E
#define B_00101111 2F
#define B_00110000 30
#define B_00110001 31
#define B_00110010 32
#define B_00110011 33
#define B_00110100 34
#define B_00110101 35
#define B_00110110 36
#define B_00110111 37
#define B_00111000 38
#define B_00111001 39
#define B_00111010 3A
#define B_00111011 3B
#define B_00111100 3C
#define B_00111101 3D
#define B_00111110 3E
#define B_00111111 3F
#define B_01000000 40
#define B_01000001 41
#define B_01000010 42
#define B_01000011 43
#define B_01000100 44
#define B_01000101 45
#define B_01000110 46
#define B_01000111 47
#define B_01001000 48
#define B_01001001 49
#define B_01001010 4A
#define B_01001011 4B
#define B_01001100 4C
#define B_01001101 4D
#define B_01001110 4E
#define B_01001111 4F
#define B_01010000 50
#define B_01010001 51
#define B_01010010 52
#define B_01010011 53
#define B_01010100 54
#define B_01010101 55
#define B_01010110 56
#define B_01010111 57
#define B_01011000 58
#define B_01011001 59
#define B_01011010 5A
#define B_01011011 5B
#define B_01011100 5C
#define B_01011101 5D
#define B_01011110 5E
#define B_01011111 5F
#define B_01100000 60
#define B_01100001 61
#define B_01100010 62
#define B_01100011 63
#define B_01100100 64
#define B_01100101 65
#define B_01100110 66
#define B_01100111 67
#define B_01101000 68
#define B_01101001 69
#define B_01101010 6A
#define B_01101011 6B
#define B_01101100 6C
#define B_01101101 6D
#define B_01101110 6E
#define B_01101111 6F
#define B_01110000 70
#define B_01110001 71
#define B_01110010 72
#define B_01110011 73
#define B_01110100 74
#define B_01110101 75
#define B_01110110 76
#define B_01110111 77
#define B_01111000 78
#define B_01111001 79
#define B_01111010 7A
#define B_01111011 7B
#define B_01111100 7C
#define B_01111101 7D
#define B_01111110 7E
#define B_01111111 7F
#define B_10000000 80
#define B_10000001 81
#define B_10000010 82
#define B_10000011 83
#define B_10000100 84
#define B_10000101 85
#define B_10000110 86
#define B_10000111 87
#define B_10001000 88
#define B_10001001 89
#define B_10001010 8A
#define B_10001011 8B
#define B_10001100 8C
#define B_10001101 8D
#define B_10001110 8E
#define B_10001111 8F
#define B_10010000 90
#define B_10010001 91
#define B_10010010 92
#define B_10010011 93
#define B_10010100 94
#define B_10010101 95
#define B_10010110 96
#define B_10010111 97
#define B_10011000 98
#define B_10011001 99
#define B_10011010 9A
#define B_10011011 9B
#define B_10011100 9C
#define B_10011101 9D
#define B_10011110 9E
#define B_10011111 9F
#define B_10100000 A0
#define B_10100001 A1
#define B_10100010 A2
#define B_10100011 A3
#define B_10100100 A4
#define B_10100101 A5
#define B_10100110 A6
#define B_10100111 A7
#define B_10101000 A8
#define B_10101001 A9
#define B_10101010 AA
#define B_10101011 AB
#define B_10101100 AC
#define B_10101101 AD
#define B_10101110 AE
#define B_10101111 AF
#define B_10110000 B0
#define B_10110001 B1
#define B_10110010 B2
#define B_10110011 B3
#define B_10110100 B4
#define B_10110101 B5
#define B_10110110 B6
#define B_10110111 B7
#define B_10111000 B8
#define B_10111001 B9
#define B_10111010 BA
#define B_10111011 BB
#define B_10111100 BC
#define B_10111101 BD
#define B_10111110 BE
#define B_10111111 BF
#define B_11000000 C0
#define B_11000001 C1
#define B_11000010 C2
#define B_11000011 C3
#define B_11000100 C4
#define B_11000101 C5
#define B_11000110 C6
#define B_11000111 C7
#define B_11001000 C8
#define B_11001001 C9
#define B_11001010 CA
#define B_11001011 CB
#define B_11001100 CC
#define B_11001101 CD
#define B_11001110 CE
#define B_11001111 CF
#define B_11010000 D0
#define B_11010001 D1
#define B_11010010 D2
#define B_11010011 D3
#define B_11010100 D4
#define B_11010101 D5
#define B_11010110 D6
#define B_11010111 D7
#define B_11011000 D8
#define B_11011001 D9
#define B_11011010 DA
#define B_11011011 DB
#define B_11011100 DC
#define B_11011101 DD
#define B_11011110 DE
#define B_11011111 DF
#define B_11100000 E0
#define B_11100001 E1
#define B_11100010 E2
#define B_11100011 E3
#define B_11100100 E4
#define B_11100101 E5
#define B_11100110 E6
#define B_11100111 E7
#define B_11101000 E8
#define B_11101001 E9
#define B_11101010 EA
#define B_11101011 EB
#define B_11101100 EC
#define B_11101101 ED
#define B_11101110 EE
#define B_11101111 EF
#define B_11110000 F0
#define B_11110001 F1
#define B_11110010 F2
#define B_11110011 F3
#define B_11110100 F4
#define B_11110101 F5
#define B_11110110 F6
#define B_11110111 F7
#define B_11111000 F8
#define B_11111001 F9
#define B_11111010 FA
#define B_11111011 FB
#define B_11111100 FC
#define B_11111101 FD
#define B_11111110 FE
#define B_11111111 FF

#define N2H_(bits) N_##bits
#define N2H(bits)  N2H_(bits)
#define B2H_(bits) B_##bits
#define B2H(bits)  B2H_(bits)
#define CON_(a, b) a##b
#define CON(a, b)  CON_(a, b) // NOLINT(bugprone-macro-parentheses)
#define HEX_(n)    0x##n
#define HEX(n)     HEX_(n)

#define WORD_04(a)          HEX(N2H(a))
#define WORD_08(a)          HEX(B2H(a))
#define WORD_16(a, b)       HEX(CON(B2H(a), B2H(b)))
#define WORD_32(a, b, c, d) HEX(CON(CON(B2H(a), B2H(b)), CON(B2H(c), B2H(d))))

// BIT-WISE macros
// clang-format off
#define SWAP_WORD_08(data) (                       \
    (((uint8_t)(data) & WORD_08(10000000)) >> 7) | \
    (((uint8_t)(data) & WORD_08(01000000)) >> 5) | \
    (((uint8_t)(data) & WORD_08(00100000)) >> 3) | \
    (((uint8_t)(data) & WORD_08(00010000)) >> 1) | \
    (((uint8_t)(data) & WORD_08(00001000)) << 1) | \
    (((uint8_t)(data) & WORD_08(00000100)) << 3) | \
    (((uint8_t)(data) & WORD_08(00000010)) << 5) | \
    (((uint8_t)(data) & WORD_08(00000001)) << 7)   \
)

#define SWAP_WORD_16(data) (                                 \
    (((uint16_t)(data) & WORD_16(00000000,11111111)) << 8) | \
    (((uint16_t)(data) & WORD_16(11111111,00000000)) >> 8)   \
)

#define SWAP_WORD_32(data) (                                                    \
    (((uint32_t)(data) & WORD_32(00000000,00000000,00000000,11111111)) << 24) | \
    (((uint32_t)(data) & WORD_32(00000000,00000000,11111111,00000000)) <<  8) | \
    (((uint32_t)(data) & WORD_32(00000000,11111111,00000000,00000000)) >>  8) | \
    (((uint32_t)(data) & WORD_32(11111111,00000000,00000000,00000000)) >> 24)   \
)
// clang-format on

#define BY_VAL(x) (uint32_t)(x)

// _gb_read_u32 / _gb_write_u32: type-safe 32-bit load/store via union.
//
// Reading through a union member other than the one last written is
// explicitly defined behaviour in C99 §6.5.2.3 and C11 §6.5.2.3 — unlike
// C++. The compiler reduces both functions to a single load/store instruction
// on every supported target. This avoids both the strict aliasing UB of
// *((uint32_t *)&x) and any dependency on <string.h> memcpy.
typedef union {
    uint32_t      u;
    unsigned char b[sizeof(uint32_t)];
} _gb_u32_pun_t;

static inline uint32_t _gb_read_u32(const void *p) {
    _gb_u32_pun_t v;
    v.b[0] = ((const unsigned char *)(p))[0];
    v.b[1] = ((const unsigned char *)(p))[1];
    v.b[2] = ((const unsigned char *)(p))[2];
    v.b[3] = ((const unsigned char *)(p))[3];
    return v.u;
}

static inline void _gb_write_u32(void *p, uint32_t val) {
    _gb_u32_pun_t v;
    v.u                       = val;
    ((unsigned char *)(p))[0] = v.b[0];
    ((unsigned char *)(p))[1] = v.b[1];
    ((unsigned char *)(p))[2] = v.b[2];
    ((unsigned char *)(p))[3] = v.b[3];
}

#define SET_MASK(mask_bit, mask_pos) (BY_VAL(mask_bit) << BY_VAL(mask_pos))
#define NOT_MASK(mask_bit, mask_pos) (~SET_MASK((mask_bit), (mask_pos)))

#define SHL_BITS(val, mask_bit, mask_pos) ((BY_VAL(val) & BY_VAL(mask_bit)) << BY_VAL(mask_pos))
#define SHR_BITS(val, mask_bit, mask_pos) ((BY_VAL(val) >> BY_VAL(mask_pos)) & BY_VAL(mask_bit))

#define SET_BITS(dst, mask_bit, mask_pos) \
    _gb_write_u32(&(dst), _gb_read_u32(&(dst)) | SET_MASK((mask_bit), (mask_pos)))
#define CLS_BITS(dst, mask_bit, mask_pos) \
    _gb_write_u32(&(dst), _gb_read_u32(&(dst)) & NOT_MASK((mask_bit), (mask_pos)))

#define READ_BITS(src, mask_bit, mask_pos) SHR_BITS((src), (mask_bit), (mask_pos))
#define WRITE_BITS(dst, mask_bit, mask_pos, val) \
    _gb_write_u32(                               \
        &(dst),                                  \
        (_gb_read_u32(&(dst)) & NOT_MASK((mask_bit), (mask_pos))) | SHL_BITS((val), (mask_bit), (mask_pos)))

// MEMORY/BUFFER macros
#define GB_ARRAY_32(x) ((uint32_t *)&(x))

// WARNING: The following macros do not check for NULL pointers and assume that
//          the destination buffer is large enough to hold the copied data. Use
//          with caution and for short buffers!

#define GB_COPY_08(d, s, n)                                                       \
    {                                                                             \
        const size_t N = (size_t)(n);                                             \
        for (size_t i = 0; i < N; ++i) ((uint8_t *)(d))[i] = ((uint8_t *)(s))[i]; \
    }

#define GB_COPY_16(d, s, n)                                                         \
    {                                                                               \
        const size_t N = (size_t)(n);                                               \
        for (size_t i = 0; i < N; ++i) ((uint16_t *)(d))[i] = ((uint16_t *)(s))[i]; \
    }

#define GB_COPY_32(d, s, n)                                                         \
    {                                                                               \
        const size_t N = (size_t)(n);                                               \
        for (size_t i = 0; i < N; ++i) ((uint32_t *)(d))[i] = ((uint32_t *)(s))[i]; \
    }

#define GB_SET_08(x, s)                                                     \
    {                                                                       \
        const size_t N = sizeof(x);                                         \
        for (size_t i = 0; i < N; ++i) ((uint8_t *)&(x))[i] = (uint8_t)(s); \
    }

#define GB_SET_16(x, s)                                                       \
    {                                                                         \
        const size_t N = sizeof(x) / 2;                                       \
        for (size_t i = 0; i < N; ++i) ((uint16_t *)&(x))[i] = (uint16_t)(s); \
    }

#define GB_SET_32(x, s)                                                       \
    {                                                                         \
        const size_t N = sizeof(x) / 4;                                       \
        for (size_t i = 0; i < N; ++i) ((uint32_t *)&(x))[i] = (uint32_t)(s); \
    }

#define GB_ZEROS_08(x)                                           \
    {                                                            \
        const size_t N = sizeof(x);                              \
        for (size_t i = 0; i < N; ++i) ((uint8_t *)&(x))[i] = 0; \
    }

#define GB_ZEROS_16(x)                                            \
    {                                                             \
        const size_t N = sizeof(x) / 2;                           \
        for (size_t i = 0; i < N; ++i) ((uint16_t *)&(x))[i] = 0; \
    }

#define GB_ZEROS_32(x)                                            \
    {                                                             \
        const size_t N = sizeof(x) / 4;                           \
        for (size_t i = 0; i < N; ++i) ((uint32_t *)&(x))[i] = 0; \
    }

#define SIZE_OF(x) (sizeof(x) / sizeof((x)[0]))

// MATH macros
#define GB_MIN(a, b) ((a) < (b) ? (a) : (b))
#define GB_MAX(a, b) ((a) > (b) ? (a) : (b))

#define GB_MOD_INC(x, y) ((x) = ((x) + 1) % (y))
#define GB_MOD_DEC(x, y) ((x) = ((x) - 1 + (y)) % (y))

// Endianness-aware zero detection
// Use standard endianness detection or fallback to runtime check
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define GB_BIG_ENDIAN 1
#else
#define GB_BIG_ENDIAN 0
#endif
#else
// Runtime detection fallback — evaluated once at file scope so the value
// is available as a constant to the compiler's dead-code eliminator.
// Using a function-call macro instead would re-evaluate on every expansion
// in inlined hot-path code.
static inline int _gb_detect_big_endian(void) {
    union {
        uint32_t      u;
        unsigned char b[4];
    } probe = {0x01000000UL};

    return probe.b[0] == 1;
}

static const int GB_BIG_ENDIAN = _gb_detect_big_endian();
#endif

// ---------------------------------------------------------------------------
// Word-width selection
//
// On 32-bit targets (or when GB_FORCE_WORD32 is defined) gb_word_t is
// uint32_t.  On 64-bit targets (or when GB_FORCE_WORD64 is defined)
// gb_word_t is uint64_t, doubling the bytes processed per loop iteration
// in all word-at-a-time hot paths (memcpy, memset, strlen, strchr, …).
//
// Override auto-detection from the build system:
//   -DGB_FORCE_WORD32   always use 32-bit words (e.g. on a 64-bit host
//                       cross-compiling for a 32-bit embedded target)
//   -DGB_FORCE_WORD64   always use 64-bit words
// ---------------------------------------------------------------------------
#if defined(GB_FORCE_WORD32)
typedef uint32_t gb_word_t;
#define GB_WORD_BITS 32
#elif defined(GB_FORCE_WORD64)
typedef uint64_t gb_word_t;
#define GB_WORD_BITS 64
#elif UINTPTR_MAX == 0xFFFFFFFFUL
typedef uint32_t gb_word_t;
#define GB_WORD_BITS 32
#else
typedef uint64_t gb_word_t;
#define GB_WORD_BITS 64
#endif

// Broadcast a single byte value into every byte lane of a gb_word_t.
// e.g. 0xAB -> 0xABABABAB (32-bit) or 0xABABABABABABABAB (64-bit).
// Used to build the cmask for word-at-a-time character searches.
#if GB_WORD_BITS == 64
#define GB_BROADCAST_BYTE(uc) ((gb_word_t)(unsigned char)(uc) * 0x0101010101010101ULL)
#else
#define GB_BROADCAST_BYTE(uc) ((gb_word_t)(unsigned char)(uc) * 0x01010101UL)
#endif

// GB_HAS_ZERO(x): detects whether any byte lane of word x is zero.
//
// Formula: ((x - 0x0101...01) & ~x & 0x8080...80)
//   - subtracting 0x01 from a zero byte borrows into the high bit
//   - masking with ~x excludes non-zero bytes that also have the high bit set
//   - masking with 0x80...80 isolates the high bit of each lane
//
// Endianness is irrelevant here: the formula tests all byte lanes of the
// word VALUE simultaneously and does not depend on byte order in memory.
// (Endianness only matters for _zero_byte_offset, which identifies WHICH
// byte is zero, not WHETHER any byte is zero.)
#if GB_WORD_BITS == 64
#define GB_HAS_ZERO(x) (((x) - 0x0101010101010101ULL) & ~(x) & 0x8080808080808080ULL)
#else
#define GB_HAS_ZERO(x) (((x) - 0x01010101U) & ~(x) & 0x80808080U)
#endif

#define GB_IS_POWER_OF_2(x) ((x) > 0 && ((x) & ((x) - 1)) == 0)

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
 * @param[out] dst Pointer to the destination memory area.
 * @param[in] src Pointer to the source memory area.
 * @param[in] len Number of bytes to copy.
 *
 * @return A pointer to the destination memory area `dst`.
 */
void *gb_memcpy(void *restrict dst,       //
                const void *restrict src, //
                size_t len);

/**
 * @brief Moves a block of memory from source to destination, handling overlaps.
 *
 * This function copies `len` bytes from the memory area pointed to by `src` to
 * the memory area pointed to by `dst`, correctly handling the case where the
 * two regions overlap. A backward copy is used only when dst falls strictly
 * inside [src, src+len); in all other cases a forward copy via gb_memcpy is
 * performed.
 *
 * @param[out] dst Pointer to the destination memory area.
 * @param[in] src Pointer to the source memory area.
 * @param[in] len Number of bytes to move.
 *
 * @return A pointer to the destination memory area `dst`.
 */
void *gb_memmove(void       *dst, //
                 const void *src, //
                 size_t      len);

/**
 * @brief Sets a block of memory to a specified value.
 *
 * This function sets the first `len` bytes of the memory area pointed to by
 * `dst` to the specified value `val`. It uses an 8-unrolled 32-bit word loop
 * to optimize the setting of memory.
 *
 * @param[out] dst Pointer to the memory area to be set.
 * @param[in] val The value to be set. The value is passed as an int, but the
 * function fills the memory block using the unsigned char conversion of this
 * value.
 * @param[in] len Number of bytes to be set to the value.
 *
 * @return A pointer to the memory area `dst`.
 */
void *gb_memset(void  *dst, //
                int    val, //
                size_t len);

/**
 * @brief Sets a block of memory to zero.
 *
 * Delegates to gb_memset(dst, 0, len) so that both functions share a single
 * optimized implementation.
 *
 * @param[out] dst Pointer to the memory block to be zeroed.
 * @param[in] len Number of bytes to set to zero.
 */
void gb_bzero(void  *dst, //
              size_t len);

/**
 * @brief Searches a memory block for the first occurrence of a byte value.
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
                size_t      len);

/**
 * @brief Searches a memory block for the last occurrence of a byte value.
 *
 * Scans backwards from byte `len - 1` to byte 0.
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
                 size_t      len);

// --- Memory — allocation --------------------------------------------------

/**
 * @brief Allocates an aligned block of memory.
 *
 * Allocates `size` bytes with a start address that is a multiple of `align`.
 * The alignment must be a non-zero power of two and at most 128; values
 * smaller than `sizeof(void *)` are silently raised to that minimum.
 *
 * The 128-byte cap exists because the back-offset between the returned
 * pointer and the raw `malloc` block is stored in a single `uint8_t`
 * immediately before the payload. With align ≤ 128 the offset fits in
 * [1, 128] with no wrapping; align = 256 would produce offset = 256,
 * which wraps to 0 in a `uint8_t` and corrupts the heap in `gb_free`.
 *
 * @param[in] size  Number of bytes to allocate.
 * @param[in] align Alignment in bytes (must be a power of two, ≤ 128).
 *
 * @return Pointer to the aligned memory block, or NULL on failure.
 */
void *gb_malloc(size_t size, //
                size_t align);

/**
 * @brief Frees a block allocated by gb_malloc.
 *
 * @warning Passing a pointer not obtained from `gb_malloc` is undefined
 * behavior and will corrupt the heap. NULL is silently ignored.
 *
 * @param[in] ptr Pointer previously returned by gb_malloc, or NULL (no-op).
 */
void gb_free(void *ptr);

// --- String — length ------------------------------------------------------

/**
 * @brief Calculates the length of a string.
 *
 * This function calculates the length of the null-terminated string `str`.
 *
 * @param[in] str Pointer to the string.
 *
 * @return The length of the string, excluding the null terminator.
 */
size_t gb_strlen(const char *str);

/**
 * @brief Calculates the length of a string, up to a maximum limit.
 *
 * Returns the length of `str`, but at most `maxlen`. If no null terminator is
 * found within the first `maxlen` bytes, `maxlen` is returned.
 *
 * @param[in] str    Pointer to the string.
 * @param[in] maxlen Maximum number of bytes to examine.
 *
 * @return The length of `str` if it is shorter than `maxlen`, otherwise
 * `maxlen`.
 */
size_t gb_strnlen(const char *str, //
                  size_t      maxlen);

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
 * @param[out] dst Pointer to the destination buffer.
 * @param[in] src Pointer to the source string.
 *
 * @return A pointer to the destination buffer.
 */
char *gb_strcpy(char *restrict dst, //
                const char *restrict src);

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
char *gb_strncpy(char *restrict dst,       //
                 const char *restrict src, //
                 size_t n);

/**
 * @brief Copies a string to a destination buffer with size-bound and
 * guaranteed null termination.
 *
 * Copies at most `size - 1` characters from `src` to `dst`, always appending
 * a null terminator if `size` is greater than zero.
 *
 * @note Truncation occurred if the return value is >= `size`.
 *
 * @param[out] dst  Pointer to the destination buffer.
 * @param[in]  src  Pointer to the source string.
 * @param[in]  size Total size of the destination buffer in bytes.
 *
 * @return The length of `src` (as if computed by gb_strlen), regardless of
 * truncation.
 */
size_t gb_strlcpy(char *restrict dst,       //
                  const char *restrict src, //
                  size_t size);

/**
 * @brief Appends a string to a destination buffer with size-bound and
 * guaranteed null termination.
 *
 * Appends characters from `src` to the null-terminated string in `dst`,
 * ensuring the total length never exceeds `size - 1` characters.
 *
 * @note Truncation occurred if the return value is >= `size`.
 *
 * @param[in,out] dst  Pointer to the destination buffer (null-terminated).
 * @param[in]     src  Pointer to the source string to append.
 * @param[in]     size Total size of the destination buffer in bytes.
 *
 * @return gb_strlen(initial dst) + gb_strlen(src), regardless of truncation.
 */
size_t gb_strlcat(char *restrict dst,       //
                  const char *restrict src, //
                  size_t size);

/**
 * @brief Duplicates a string into a newly allocated buffer.
 *
 * Allocates exactly gb_strlen(str) + 1 bytes, copies the string content, and
 * appends the null terminator. The returned pointer must be freed by the caller.
 *
 * @param[in] str Pointer to the null-terminated string to duplicate.
 *
 * @return Pointer to the newly allocated duplicate string, or NULL if
 * allocation fails or `str` is NULL.
 */
char *gb_strdup(const char *str);

// --- String — search ------------------------------------------------------

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
char *gb_strchr(const char *str, //
                int         c);

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
char *gb_strrchr(const char *str, //
                 int         c);

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
char *gb_strstr(const char *haystack, //
                const char *needle);

/**
 * @brief Finds the first occurrence of any character from a set of characters.
 *
 * This function scans the null-terminated string `str` for the first
 * occurrence of any character from the null-terminated string `reject`.
 *
 * @note Returns 0 on NULL input. Standard strcspn has undefined behavior on
 * NULL; this function returns 0 defensively in that case.
 *
 * @param[in] str Pointer to the null-terminated string to search.
 * @param[in] reject Pointer to the null-terminated string containing characters to reject.
 *
 * @return The number of characters at the beginning of `str` that are not
 * in the `reject` string.
 */
size_t gb_strcspn(const char *str, //
                  const char *reject);

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
char *gb_strtok_r(char *restrict str,         //
                  const char *restrict delim, //
                  char **restrict saveptr);

// --- String — compare -----------------------------------------------------

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
int gb_strcmp(const char *str1, //
              const char *str2);

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
int gb_strncmp(const char *str1, //
               const char *str2, //
               size_t      n);

// --- Conversion -----------------------------------------------------------

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
bool gb_bin2dec(const char *restrict src_bin, //
                char *restrict dst_dec,       //
                size_t dst_len);

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
bool gb_bin2hex(const char *restrict src_bin, //
                char *restrict dst_hex,       //
                size_t dst_len);

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
bool gb_dec2bin(const char *restrict src_dec, //
                char *restrict dst_bin,       //
                size_t dst_len);

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
bool gb_dec2hex(const char *restrict src_dec, //
                char *restrict dst_hex,       //
                size_t dst_len);

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
bool gb_hex2bin(const char *restrict src_hex, //
                char *restrict dst_bin,       //
                size_t dst_len);

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
bool gb_hex2dec(const char *restrict src_hex, //
                char *restrict dst_dec,       //
                size_t dst_len);

/**
 * @brief Converts a raw binary byte buffer to its hexadecimal string
 * representation.
 *
 * This function takes a buffer of raw bytes and converts it into a
 * null-terminated string of hexadecimal characters. Each byte in the source
 * buffer is represented by two characters in the destination string.
 *
 * @param[in] src_buf Pointer to the source buffer containing the raw binary data.
 * @param[in] src_len The number of bytes in the source buffer to convert.
 * @param[out] dst_str Pointer to the destination buffer where the resulting
 * hexadecimal string will be stored.
 * @param[in] dst_len The size of the destination buffer in bytes. Must be at
 * least (src_len * 2) + 1 to accommodate the full string and the null terminator.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2str(const char *restrict src_buf, //
                size_t src_len,               //
                char *restrict dst_str,       //
                size_t dst_len);

/**
 * @brief Converts a raw binary byte buffer to its hexadecimal string
 * representation, with byte reversal.
 *
 * Same nibble-splitting algorithm as gb_hex2str, but bytes are read from
 * last to first, producing a byte-reversed (endian-swapped) output string.
 *
 * For example, the input buffer {0x1A, 0x2B, 0x3C} yields "3C2B1A".
 *
 * @param[in] src_buf Pointer to the source buffer containing the raw binary data.
 * @param[in] src_len The number of bytes in the source buffer to convert.
 * @param[out] dst_str Pointer to the destination buffer where the resulting
 * hexadecimal string will be stored.
 * @param[in] dst_len The size of the destination buffer in bytes. Must be at
 * least (src_len * 2) + 1 to accommodate the full string and the null terminator.
 *
 * @return `true` if the conversion was successful, `false` otherwise.
 */
bool gb_hex2str_r(const char *restrict src_buf, //
                  size_t src_len,               //
                  char *restrict dst_str,       //
                  size_t dst_len);

#endif // GB_UTILS_H

/* *****************************************************************************
 End of File
 */
