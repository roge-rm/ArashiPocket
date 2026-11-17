/* Fixed-width types and big-endian byte helpers for the core. Plain C89, no
   platform headers. int may be 16 bit (Palm) or 32 bit (PC, Windows CE), so
   anything stored or exchanged uses these. */
#ifndef CORE_TYPES_H
#define CORE_TYPES_H

typedef signed char    int8;
typedef unsigned char  uint8;
typedef short          int16;
typedef unsigned short uint16;

/* long is 32 bit on Palm and Windows CE, 64 bit on 64-bit Linux, where int
   is 32 bit. */
#if defined(__LP64__) || defined(_LP64)
typedef int            int32;
typedef unsigned int   uint32;
#else
typedef long           int32;
typedef unsigned long  uint32;
#endif

typedef int32 Fixed;      /* 16.16 */
typedef int32 Fract;      /* 2.30 */
typedef uint8 cbool;

#define fixed1     ((Fixed)0x00010000L)
#define int2fix(i) ((Fixed)(i) << 16)
#define fix2int(f) ((int16)((f) >> 16))

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Big-endian reads from resource bytes, which have no alignment. */
#define get16(p) ((uint16)(((uint16)((const uint8 *)(p))[0] << 8) | \
                           ((const uint8 *)(p))[1]))
#define get32(p) (((uint32)((const uint8 *)(p))[0] << 24) | \
                  ((uint32)((const uint8 *)(p))[1] << 16) | \
                  ((uint32)((const uint8 *)(p))[2] << 8) | \
                  (uint32)((const uint8 *)(p))[3])
#define gets16(p) ((int16)get16(p))
#define gets32(p) ((int32)get32(p))

/* Four-character code, as in 'TEXT'. */
#define FOURCC(a, b, c, d) (((uint32)(uint8)(a) << 24) | ((uint32)(uint8)(b) << 16) | \
                            ((uint32)(uint8)(c) << 8) | (uint32)(uint8)(d))

#endif
