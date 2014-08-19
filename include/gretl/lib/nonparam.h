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

#ifndef NONPARAM_H
#define NONPARAM_H

#include <gretl/lib/libgretl.h>
 
int spearman_rho (const int *list, 
		  const DATASET *dset,
		  gretlopt opt, PRN *prn);

int kendall_tau (const int *list, 
		 const DATASET *dset,
		 gretlopt opt, PRN *prn);

double lockes_test (const double *x, int t1, int t2);

int runs_test (int v, const DATASET *dset, 
	       gretlopt opt, PRN *prn);

int diff_test (const int *list, const DATASET *dset, 
	       gretlopt opt, PRN *prn);

int sort_pairs_by_x (gretl_matrix *x, gretl_matrix *y, int **order,
		     char **labels);

gretl_matrix *loess_fit (const gretl_matrix *x, const gretl_matrix *y,
			 int d, double q, gretlopt opt, int *err);

#endif /* NONPARAM_H */

