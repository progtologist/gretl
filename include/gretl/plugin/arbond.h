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

#ifndef ARBOND_H
#define ARBOND_H

#define ADEBUG 0
#define WRITE_MATRICES 0

enum {
    DPD_TWOSTEP  = 1 << 0,
    DPD_ORTHDEV  = 1 << 1,
    DPD_TIMEDUM  = 1 << 2,
    DPD_WINCORR  = 1 << 3,
    DPD_SYSTEM   = 1 << 4,
    DPD_DPDSTYLE = 1 << 5,
    DPD_REDO     = 1 << 6
};

#define gmm_sys(d) (d->flags & DPD_SYSTEM)
#define dpd_style(d) (d->flags & DPD_DPDSTYLE)

#define LEVEL_ONLY 2

#define data_index(dpd,i) (i * dpd->T + dpd->t1)

#include <gretl/lib/libgretl.h>
#include <gretl/lib/version.h>
#include <gretl/lib/matrix_extra.h>

typedef struct ddset_ ddset;
typedef struct unit_info_ unit_info;
typedef struct diag_info_ diag_info;

struct unit_info_ {
    int t1;      /* first usable obs in differences for unit */
    int t2;      /* last usable obs */
    int nobs;    /* number of usable observations (in the system case
            this is the sum of the differenced and level
            observations */
    int nlev;    /* number of obs in levels (0 in non-system case) */
};

struct diag_info_ {
    int v;       /* ID number of variable */
    int depvar;  /* is the target variable the dependent variable (1/0) */
    int minlag;  /* minimum lag order */
    int maxlag;  /* maximum lag order */
    int level;   /* instrument spec is for levels */
    int rows;    /* max rows occupied in Zi (for dpanel only) */
    int tbase;   /* first obs with potentially available instruments */
};

struct ddset_ {
    int ci;               /* ARBOND or DPANEL */
    int flags;            /* option flags */
    int step;             /* what step are we on? (1 or 2) */
    int yno;              /* ID number of dependent var */
    int p;                /* lag order for dependent variable */
    int qmax;             /* longest lag of y used as instrument (arbond) */
    int nx;               /* number of independent variables */
    int ifc;              /* model includes a constant */
    int nzr;              /* number of regular instruments */
    int nzb;              /* number of block-diagonal instruments (other than y) */
    int nz;               /* total columns in instrument matrix */
    int pc0;              /* column in Z where predet vars start */
    int xc0;              /* column in Z where exog vars start */
    int N;                /* total number of units in sample */
    int effN;             /* number of units with usable observations */
    int T;                /* total number of observations per unit */
    int minTi;            /* minimum equations (> 0) for any given unit */
    int maxTi;            /* maximum diff. equations for any given unit */
    int max_ni;           /* max number of (possibly stacked) obs per unit
                 (equals maxTi except in system case) */
    int k;                /* number of parameters estimated */
    int nobs;             /* total observations actually used (diffs or levels) */
    int totobs;           /* total observations (diffs + levels) */
    int t1;               /* initial offset into dataset */
    int t1min;            /* first usable observation, any unit */
    int t2max;            /* last usable observation, any unit */
    int ndum;             /* number of time dummies to use */
    double SSR;           /* sum of squared residuals */
    double s2;            /* residual variance */
    double AR1;           /* z statistic for AR(1) errors */
    double AR2;           /* z statistic for AR(2) errors */
    double sargan;        /* overidentification test statistic */
    double wald[2];       /* Wald test statistic(s) */
    int wdf[2];           /* degrees of freedom for Wald test(s) */
    int *xlist;           /* list of independent variables */
    int *ilist;           /* list of regular instruments */
    gretl_matrix_block *B1; /* matrix holder */
    gretl_matrix_block *B2; /* matrix holder */
    gretl_matrix *beta;   /* parameter estimates */
    gretl_matrix *vbeta;  /* parameter variance matrix */
    gretl_matrix *uhat;   /* residuals, differenced version */
    gretl_matrix *H;      /* step 1 error covariance matrix */
    gretl_matrix *A;      /* \sum Z'_i H Z_i */
    gretl_matrix *Acpy;   /* back-up of A matrix */
    gretl_matrix *V;      /* covariance matrix */
    gretl_matrix *ZT;     /* transpose of full instrument matrix */
    gretl_matrix *Zi;     /* per-unit instrument matrix */
    gretl_matrix *Y;      /* transformed dependent var */
    gretl_matrix *X;      /* lagged differences of y, indep vars, etc. */
    gretl_matrix *kmtmp;  /* workspace */
    gretl_matrix *kktmp;
    gretl_matrix *M;
    gretl_matrix *L1;
    gretl_matrix *XZA;
    gretl_matrix *ZY;
    gretl_matrix *XZ;
    diag_info *d;          /* info on block-diagonal instruments */
    unit_info *ui;         /* info on panel units */
    char *used;            /* global record of observations used */

    /* The members below are specific to the "dpanel" approach */

    int ndiff;             /* total differenced observations */
    int nlev;              /* total levels observations */
    int nzb2;              /* number of block-diagonal specs, levels eqns */
    int nzdiff;            /* number of insts specific to eqns in differences */
    int nzlev;             /* number of insts specific to eqns in levels */
    int *laglist;          /* (possibly discontinuous) list of lags */
    diag_info *d2;         /* info on block-diagonal instruments, levels eqns
                  (note: not independently allocated) */
    int dcols;             /* number of columns, differenced data */
    int dcolskip;          /* initial number of skipped obs, differences */
    int lcol0;             /* column adjustment for data in levels */
    int lcolskip;          /* initial number of skipped obs, levels */
};

void dpanel_residuals (ddset *dpd);

int dpd_process_list (ddset *dpd, const int *list,
                 const int *ylags);

void ddset_free (ddset *dpd);

int dpd_allocate_matrices (ddset *dpd);

int dpd_add_unit_info (ddset *dpd);

int dpd_flags_from_opt (gretlopt opt);

ddset *ddset_new (int ci, const int *list, const int *ylags,
             const DATASET *dset, gretlopt opt,
             diag_info *d, int nzb, int *err);

/* if the const has been included among the regressors but not the
   instruments, add it to the instruments */

int maybe_add_const_to_ilist (ddset *dpd);

int dpd_make_lists (ddset *dpd, const int *list, int xpos);

int dpanel_make_laglist (ddset *dpd, const int *list,
                int seppos, const int *ylags);

/* Process the incoming list and figure out various parameters
   including the maximum lag of the dependent variable, dpd->p, and
   the number of exogenous regressors, dpd->nx.

   Note that the specification of the first sublist (before the first
   LISTSEP) differs between arbond and dpanel.

   In arbond this chunk contains either p alone or p plus "qmax", a
   limiter for the maximum lag of y to be used as an instrument.

   In dpanel, it contains either a single integer value for p or a set
   of specific lags of y to use as regressors (given in the form of
   integers in braces or as the name of a matrix). In the former case
   the auxiliary @ylags list will be NULL; in the latter @ylags
   records the specific lags requested. At the user level, therefore,

   dpanel 2 ; y ...   means use lags 1 and 2
   dpanel {2} ; y ... means use lag 2 only

   Note that placing a limit on the max lag of y _as instrument_ is
   done in dpanel via "GMM(y,min,max)".
*/

int dpd_process_list (ddset *dpd, const int *list,
                 const int *ylags);

/* The following several book-keeping functions are specific
   to the arbond command */

/* See if we have valid values for the dependent variable (in
   differenced or deviations form) plus p lags of same, and all of
   the independent variables, at the given observation, s.
   TODO: either generalize this or add a parallel function
   for handling equations in levels.
*/

int obs_is_usable (ddset *dpd, const DATASET *dset, int s);

int bz_columns (ddset *dpd, int i);

/* find the first y observation, all units */

int arbond_find_t1min (ddset *dpd, const double *y);

/* check the sample for unit/individual i */

int
arbond_sample_check_unit (ddset *dpd, const DATASET *dset, int i,
              int s, int *t1imin, int *t2max);

/* compute the column-structure of the matrix Zi */

void arbond_compute_Z_cols (ddset *dpd, int t1min, int t2max);

int arbond_sample_check (ddset *dpd, const DATASET *dset);

/* end of arbond book-keeping block */

/* Figure the 0-based index of the position of the constant
   in the coefficient vector (or return -1 if the constant is
   not included).
*/

int dpd_const_pos (ddset *dpd);

/* We do either one or two tests here: first we test for the joint
   significance of all regular regressors (lags of y plus any
   exogenous variables, except for the constant, if present).
   Then, if time dummies are present, we test for their joint
   significance.
*/

int dpd_wald_test (ddset *dpd);

int dpd_sargan_test (ddset *dpd);

/* \sigma^2 H_1, sliced and diced for unit i */

void make_asy_Hi (ddset *dpd, int i, gretl_matrix *H,
             char *mask);

/*
  AR(1) and AR(2) tests for residuals, see
  Doornik, Arellano and Bond, DPD manual (2006), pp. 28-29.

  AR(m) = \frac{d_0}{\sqrt{d_1 + d_2 + d_3}}

  d_0 = \sum_i w_i' u_i

  d_1 = \sum_i w_i' H_i w_i

  d_2 = -2(\sum_i w_i' X_i) M^{-1}
        (\sum_i X_i' Z_i) A_N (\sum Z_i' H_i w_i)

  d_3 = (\sum_i w_i' X_i) var(\hat{\beta}) (\sum_i X_i' w_i)

  The matrices with subscript i in the above equations are all
  in differences. In the "system" case the last term in d2 is
  modified as

  (\sum Z_i^f' u_i^f u_i' w_i)

  where the f superscript indicates that all observations are
  used, differences and levels.

  one step:         H_i = \sigma^2 H_{1,i}
  one step, robust: H_i = H_{2,i}; M = M_1
  two step:         H_i = H_{2,i}; M = M_2

  H_{2,i} = v^*_{1,i} v^*_{1,i}'

*/

int dpd_ar_test (ddset *dpd);

/* Windmeijer, Journal of Econometrics, 126 (2005), page 33:
   finite-sample correction for step-2 variance matrix.  Note: from a
   computational point of view this calculation is expressed more
   clearly in the working paper by Bond and Windmeijer, "Finite Sample
   Inference for GMM Estimators in Linear Panel Data Models" (2002),
   in particular the fact that the matrix named "D" below must be
   built column by column, taking the derivative of the "W" (or "A")
   matrix with respect to the successive independent variables.
*/

int windmeijer_correct (ddset *dpd, const gretl_matrix *uhat1,
                   const gretl_matrix *varb1);

/* second-step (asymptotic) variance */

int dpd_variance_2 (ddset *dpd,
               gretl_matrix *u1,
               gretl_matrix *V1);

/*
   Compute the robust step-1 robust variance matrix:

   N * M^{-1} * (X'*Z*A_N*\hat{V}_N*A_N*Z'*X) * M^{-1}

   where

   M = X'*Z*A_N*Z'*X

   and

   \hat{V}_N = N^{-1} \sum Z_i'*v_i*v_i'*Z_i,

   (v_i being the step-1 residuals).

   (But if we're doing 1-step and get the asymptotic
   flag, just do the simple thing, \sigma^2 M^{-1})
*/

int dpd_variance_1 (ddset *dpd);

/* no "system" case here: all residuals are in differences */

void arbond_residuals (ddset *dpd);

/* In the "system" case dpd->uhat contains both differenced
   residuals and levels residuals (stacked per unit). In that case
   trim the vector down so that it contains only one set of
   residuals prior to transcribing in series form. Which set
   we preserve is governed by @save_levels.
*/

int dpanel_adjust_uhat (ddset *dpd,
                   const DATASET *dset,
                   int save_levels);

int dpd_finalize_model (MODEL *pmod, ddset *dpd,
                   const int *list, const int *ylags,
                   const char *istr,
                   const DATASET *dset,
                   gretlopt opt);

/* exclude any independent variables that are zero at all
   relevant observations */

int dpd_zero_check (ddset *dpd, const DATASET *dset);

/* Based on reduction of the A matrix, trim ZT to match and
   adjust the sizes of all workspace matrices that have a
   dimension involving dpd->nz. Note that this is called
   on the first step only, and it is called before computing
   XZ' and Z'Y. The only matrices we need to actually "cut"
   are A (already done when we get here) and ZT; XZ' and Z'Y
   should just have their nz dimension changed.
*/

void dpd_shrink_matrices (ddset *dpd, const char *mask);

int dpd_step_2_A (ddset *dpd);

/* compute \hat{\beta} from the moment matrices */

int dpd_beta_hat (ddset *dpd);

/* recompute \hat{\beta} and its variance matrix */

int dpd_step_2 (ddset *dpd);

/* arbond-specific functions follow */

double odev_at_lag (const double *x, int t, int lag, int pd);

int arbond_make_y_X (ddset *dpd, const DATASET *dset);

int next_obs (ddset *dpd, int t0, int j0, int n);

/* construct the H matrix for first-differencing
   as applied to unit i */

void arbond_H_matrix (ddset *dpd, int *rc,
                 int i, int t0);

int arbond_make_Z_and_A (ddset *dpd, const DATASET *dset);

/* This function is used on the first step (only), by both
   arbond and dpanel. */

int dpd_invert_A_N (ddset *dpd);

int dpd_step_1 (ddset *dpd);

int diag_try_list (const char *vname, int *vp, int *nd,
              diag_info **dp, int **listp);

/* Parse a particular entry in the (optional) incoming array
   of "GMM(foo,m1,m2)" specifications.  We check that foo
   exists and that m1 and m2 have sane values (allowing that,
   for arbond, an m2 value of 0 means use all available lags).
*/

int parse_diag_info (int ci, const char *s, diag_info **dp,
                int *ip, int *nd, const DATASET *dset);

/* parse requests of the form

      GMM(xvar, minlag, maxlag)

   which call for inclusion of lags of xvar in block-diagonal
   fashion
*/

int
parse_GMM_instrument_spec (int ci, const char *spec, const DATASET *dset,
               diag_info **pd, int *pnspec);

/* public interface: driver for Arellano-Bond type estimation */

MODEL
arbond_estimate (const int *list, const char *ispec,
         const DATASET *dset, gretlopt opt,
         PRN *prn);

#endif // ARBOND_H
