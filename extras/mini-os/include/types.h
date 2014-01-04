/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*-
 ****************************************************************************
 * (C) 2003 - Rolf Neugebauer - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: types.h
 *      Author: Rolf Neugebauer (neugebar@dcs.gla.ac.uk)
 *     Changes: 
 *              
 *        Date: May 2003
 * 
 * Environment: Xen Minimal OS
 * Description: a random collection of type definitions
 *
 ****************************************************************************
 * $Id: h-insert.h,v 1.4 2002/11/08 16:03:55 rn Exp $
 ****************************************************************************
 */

#ifndef _TYPES_H_
#define _TYPES_H_
#include <stddef.h>

/* FreeBSD compat types */
#ifndef HAVE_LIBC
typedef unsigned char       u_char;
typedef unsigned int        u_int;
typedef unsigned long       u_long;
#endif
#if defined(__i386__) || defined(__arm__)
typedef long long           quad_t;
typedef unsigned long long  u_quad_t;

typedef struct { unsigned long pte_low, pte_high; } pte_t;

#elif defined(__x86_64__)
typedef long                quad_t;
typedef unsigned long       u_quad_t;

typedef struct { unsigned long pte; } pte_t;
#endif /* __i386__ || __x86_64__ */

#ifdef __x86_64__ || __aarch64__
#define __pte(x) ((pte_t) { (x) } )
#else
#define __pte(x) ({ unsigned long long _x = (x);        \
    ((pte_t) {(unsigned long)(_x), (unsigned long)(_x>>32)}); })
#endif

#ifdef HAVE_LIBC
#include <limits.h>
#include <stdint.h>
#else
#if defined(__i386__) || defined(__arm__)
typedef unsigned int        uintptr_t;
typedef int                 intptr_t;
#elif defined(__x86_64__) || defined(__aarch64__)
typedef unsigned long       uintptr_t;
typedef long                intptr_t;
#endif /* __i386__ || __x86_64__ */
typedef unsigned char uint8_t;
typedef   signed char int8_t;
typedef unsigned short uint16_t;
typedef   signed short int16_t;
typedef unsigned int uint32_t;
typedef   signed int int32_t;
#if defined(__i386__) || defined(__arm__)
typedef   signed long long int64_t;
typedef unsigned long long uint64_t;
#elif defined(__x86_64__) || defined(__aarch64__)
typedef   signed long int64_t;
typedef unsigned long uint64_t;
#endif
typedef uint64_t uintmax_t;
typedef  int64_t intmax_t;
typedef uint64_t off_t;
#endif

typedef intptr_t            ptrdiff_t;


#ifndef HAVE_LIBC
typedef long ssize_t;
#endif

/*
 * From
 *	@(#)quad.h	8.1 (Berkeley) 6/4/93
 */

#ifdef __BIG_ENDIAN
#define _QUAD_HIGHWORD 0
#define _QUAD_LOWWORD 1
#else /* __LITTLE_ENDIAN */
#define _QUAD_HIGHWORD 1
#define _QUAD_LOWWORD 0
#endif

/*
 * Define high and low longwords.
 */
#define H               _QUAD_HIGHWORD
#define L               _QUAD_LOWWORD

/*
 * Total number of bits in a quad_t and in the pieces that make it up.
 * These are used for shifting, and also below for halfword extraction
 * and assembly.
 */
#define CHAR_BIT        8               /* number of bits in a char */
#define QUAD_BITS       (sizeof(s64) * CHAR_BIT)
#define LONG_BITS       (sizeof(long) * CHAR_BIT)
#define HALF_BITS       (sizeof(long) * CHAR_BIT / 2)

#define B (1 << HALF_BITS) /* digit base */
/*
 * Extract high and low shortwords from longword, and move low shortword of
 * longword to upper half of long, i.e., produce the upper longword of
 * ((quad_t)(x) << (number_of_bits_in_long/2)).  (`x' must actually be u_long.)
 *
 * These are used in the multiply code, to split a longword into upper
 * and lower halves, and to reassemble a product as a quad_t, shifted left
 * (sizeof(long)*CHAR_BIT/2).
 */
#define HHALF(x)        ((x) >> HALF_BITS)
#define LHALF(x)        ((x) & ((1 << HALF_BITS) - 1))
#define LHUP(x)         ((x) << HALF_BITS)

#define COMBINE(a, b) (((u_long)(a) << HALF_BITS) | (b))

/*
 * Depending on the desired operation, we view a `long long' (aka quad_t) in
 * one or more of the following formats.
 */
union uu {
    int64_t            q;              /* as a (signed) quad */
    int64_t            uq;             /* as an unsigned quad */
    long           sl[2];          /* as two signed longs */
    unsigned long  ul[2];          /* as two unsigned longs */
};

/* select a type for digits in base B */
typedef u_long digit;

#endif /* _TYPES_H_ */
