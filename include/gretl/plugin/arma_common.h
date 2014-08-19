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
 */

#ifndef ARMA_COMMON_H
#define ARMA_COMMON_H

#include <gretl/lib/libgretl.h>
#include <gretl/lib/usermat.h>
#include <gretl/plugin/arma_priv.h>

#define MAX_ARMA_ORDER 128
#define MAX_ARIMA_DIFF 2

enum {
    AR_MASK,
    MA_MASK
};

void
real_arima_difference_series (double *dx, const double *x,
                  int t1, int t2, int *delta,
                  int k);

void
arma_info_init (arma_info *ainfo, gretlopt opt,
        const int *pqspec, const DATASET *dset);

void arma_info_cleanup (arma_info *ainfo);

/* Create a mask for skipping certain intermediate lags,
   AR or MA.  This function also sets ainfo->np and ainfo->nq,
   which record the actual number of non-seasonal AR and MA
   lags used.
*/
char *mask_from_list (const int *list,
                 arma_info *ainfo,
                 int m, int *err);

int arma_make_masks (arma_info *ainfo);

int arma_list_y_position (arma_info *ainfo);

int arima_integrate (double *dx, const double *x,
                int t1, int t2, int d, int D, int s);

void ainfo_data_to_model (arma_info *ainfo, MODEL *pmod);

void arma_depvar_stats (MODEL *pmod, arma_info *ainfo,
                   const DATASET *dset);


/* write the various statistics from ARMA estimation into
   a gretl MODEL struct */

void write_arma_model_stats (MODEL *pmod, arma_info *ainfo,
                 const DATASET *dset);

void calc_max_lag (arma_info *ainfo);

int arma_adjust_sample (arma_info *ainfo,
                   const DATASET *dset,
                   int *missv, int *misst);

/* remove the intercept from list of regressors */

int arma_remove_const (arma_info *ainfo,
                  const DATASET *dset);

int check_arma_sep (arma_info *ainfo, int sep1);

int arma_add_xlist (arma_info *ainfo, int ypos);

int check_arma_list (arma_info *ainfo,
                const DATASET *dset,
                gretlopt opt);

int check_arima_list (arma_info *ainfo,
                 const DATASET *dset,
                 gretlopt opt);

int arma_check_list (arma_info *ainfo,
                const DATASET *dset,
                gretlopt opt);

void
real_arima_difference_series (double *dx, const double *x,
                  int t1, int t2, int *delta,
                  int k);

#ifndef X12A_CODE

/* Add to the ainfo struct a full-length series y holding
   the differenced version of the dependent variable.
   If the "xdiff" flag is set on ainfo, in addition
   create a matrix dX holding the differenced regressors;
   in that case the time-series length of dX depends on
   the @fullX flag -- if fullX = 0, this equals
   ainfo->T but if fullX = 0 it equals ainfo->t2 + 1.
*/

int arima_difference (arma_info *ainfo, const DATASET *dset,
              int fullX);

void arima_difference_undo (arma_info *ainfo, const DATASET *dset);

#endif /* X12A_CODE not defined */

#endif // ARMA_COMMON_H
