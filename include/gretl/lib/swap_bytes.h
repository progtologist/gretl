/*
 *  $Id: swap_bytes.h,v 1.6 2010/01/16 20:02:47 allin Exp $
 *
 *  Reverse bytes in 2, 4 and 8 byte objects
 *
 *  Copyright 2000-2000 Saikat DebRoy <saikat@stat.wisc.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 *  MA 02111-1307, USA
 *
 */

#ifndef SWAP_BYTES_H
#define SWAP_BYTES_H

#ifdef HAVE_BYTESWAP_H  /* use GNU bswap macros */

#include <byteswap.h>

#define swap_bytes_16(from, to) do { (to) = bswap_16(from); } while (0)

#define swap_bytes_32(from, to) do { (to) = bswap_32(from); } while (0)

#if defined __GNUC__ && __GNUC__ >= 2

#define swap_bytes_double(from, to)		\
do {						\
    union {					\
	unsigned long long int u64;		\
	double d;				\
    } from__, to__;				\
    from__.d = (from);				\
    to__.u64 = bswap_64(from__.u64);		\
    (to) = to__.d;				\
} while (0)

#else

#define swap_bytes_double(from, to)		\
do {						\
    union {					\
	unsigned int u32[2];			\
	double d;				\
    } from__, to__;				\
    from__.d = (from);  			\
    swap_bytes_32(from__.u32[1], to__.u32[0]);	\
    swap_bytes_32(from__.u32[0], to__.u32[1]);	\
    (to) = to__.d;				\
} while (0)

#endif

#else /* use reasonably portable definitions */

#define swap_bytes_16(from, to)						\
do {									\
    unsigned short from__16 = (from);					\
    (to) = ((((from__16) >> 8) & 0xff) | (((from__16) & 0xff) << 8));	\
} while (0)

#define swap_bytes_32(from, to)			\
do {						\
    unsigned int from__32 = (from);		\
    (to) = (((from__32 & 0xff000000) >> 24) |	\
	    ((from__32 & 0x00ff0000) >>  8) |	\
	    ((from__32 & 0x0000ff00) <<  8) |	\
	    ((from__32 & 0x000000ff) << 24));	\
} while (0)

#define swap_bytes_double(from, to)		\
do {						\
    union {					\
	unsigned int u32[2];			\
	double d;				\
    } from__, to__;				\
    from__.d = (from);  			\
    swap_bytes_32(from__.u32[1], to__.u32[0]);	\
    swap_bytes_32(from__.u32[0], to__.u32[1]);	\
    (to) = to__.d;				\
} while (0)

#endif  /* HAVE_BYTESWAP_H or not */

#define swap_bytes_ushort(from, to) swap_bytes_16(from, to)

#define reverse_ushort(x) swap_bytes_16(x, x)

#define swap_bytes_short(from, to)              \
do {						\
    union {					\
	unsigned short u16;			\
	short          s16;			\
    } from__, to__;				\
    from__.s16 = (from);			\
    swap_bytes_16(from__.u16, to__.u16);	\
    (to) = to__.s16;				\
} while (0)

#define reverse_short(x)                        \
do {						\
    union {					\
	unsigned short u16;			\
	short          s16;			\
    } from__, to__;				\
    from__.s16 = (x);    			\
    swap_bytes_16(from__.u16, to__.u16);	\
    (x) = to__.s16;				\
} while (0)

#define swap_bytes_uint(from, to) swap_bytes_32(from, to)

#define reverse_uint(x) swap_bytes_32(x, x)

#define swap_bytes_int(from, to)                \
do {						\
    union {					\
	unsigned int u32;			\
	int          s32;			\
    } from__, to__;				\
    from__.s32 = (from);			\
    swap_bytes_32(from__.u32, to__.u32);	\
    (to) = to__.s32;				\
} while (0)

#define reverse_int(x)                          \
do {						\
    union {					\
	unsigned int u32;			\
	int          s32;			\
    } from__, to__;				\
    from__.s32 = (x);    			\
    swap_bytes_32(from__.u32, to__.u32);	\
    (x) = to__.s32;				\
} while (0)

#define swap_bytes_float(from, to)		\
do {						\
    union {					\
	unsigned int u32;			\
	float f;				\
    } from__, to__;				\
    from__.f = (from);				\
    swap_bytes_32(from__.u32, to__.u32);	\
    (to) = to__.f;				\
} while (0)

#define reverse_float(x)        		\
do {						\
    union {					\
	unsigned int u32;			\
	float f;				\
    } from__, to__;				\
    from__.f = (x);				\
    swap_bytes_32(from__.u32, to__.u32);	\
    (x) = to__.f;				\
} while (0)

#define reverse_double(x) swap_bytes_double(x, x)

#endif /* SWAP_BYTES_H */
