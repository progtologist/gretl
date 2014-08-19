/*
 *  gretl -- Gnu Regression, Econometrics and Time-series Library
 *  Copyright (C) 2001 Allin Cottrell and Riccardo "Jack" Lucchetti
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/* All code here is conditional on either AVX or SSE */

#ifndef MATRIX_SIMD_H
#define MATRIX_SIMD_H

#include <gretl/lib/libgretl.h>
#include <gretl/config.h>

#define SHOW_SIMD 0

#if defined(USE_AVX)
# define USE_SIMD 1
# if defined(HAVE_IMMINTRIN_H)
#  include <immintrin.h>
# else
#  include <mmintrin.h>
#  include <xmmintrin.h>
#  include <emmintrin.h>
# endif
#endif

#ifdef USE_SIMD
# if defined(USE_AVX)
#  define mval_malloc(sz) _mm_malloc(sz,32)
# else
#  define mval_malloc(sz) _mm_malloc(sz,16)
# endif
#else
# define mval_malloc(sz) malloc(sz)
#endif

#ifdef USE_SIMD
inline void mval_free (void *mem);
#else
# define mval_free(v) free(v)
#endif

double *mval_realloc (gretl_matrix *m, int n);

#ifdef USE_AVX

int gretl_matrix_simd_add_to (gretl_matrix *a,
                     const gretl_matrix *b,
                     int n);

int gretl_matrix_simd_subt_from (gretl_matrix *a,
                    const gretl_matrix *b,
                    int n);

#else /* SSE */

int gretl_matrix_simd_add_to (gretl_matrix *a,
                     const gretl_matrix *b,
                     int n);

int gretl_matrix_simd_subt_from (gretl_matrix *a,
                    const gretl_matrix *b,
                    int n);

#endif /* AVX vs SSE */

#ifdef USE_AVX

int gretl_matrix_simd_add (const double *ax,
                  const double *bx,
                  double *cx,
                  int n);

int gretl_matrix_simd_subtract (const double *ax,
                       const double *bx,
                       double *cx,
                       int n);

#else /* SSE */

int gretl_matrix_simd_add (const gretl_matrix *a,
                  const gretl_matrix *b,
                  gretl_matrix *c);

int gretl_matrix_simd_subtract (const gretl_matrix *a,
                       const gretl_matrix *b,
                       gretl_matrix *c);

#endif /* AVX vs SSE */

#ifdef USE_AVX

/* fast but restrictive: both A and B must be 4 x 4 */

int gretl_matrix_avx_mul4 (const double *aval,
                  const double *bval,
                  double *cval);

/* fast but restrictive: both A and B must be 8 x 8 */

int gretl_matrix_avx_mul8 (const double *aval,
                  const double *bval,
                  double *cval);

/* Note: this is probably usable only for k <= 8 (shortage of AVX
   registers). It is much faster if m is a multiple of 4; n is
   unconstrained.
*/

int gretl_matrix_simd_mul (const gretl_matrix *A,
                  const gretl_matrix *B,
                  gretl_matrix *C);

#else /* SSE */

/* Note: this is probably usable only for k <= 8 (shortage of SSE
   registers). It is faster if m is a multiple of 2; but n is
   unconstrained.
*/

int gretl_matrix_simd_mul (const gretl_matrix *A,
                  const gretl_matrix *B,
                  gretl_matrix *C);

#endif /* AVX vs SIMD */

#endif // MATRIX_SIMD_H
