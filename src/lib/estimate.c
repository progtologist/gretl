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

/* estimate.c - basic gretl estimation procedures */

#include <gretl/lib/libgretl.h>
#include <gretl/lib/qr_estimate.h>
#include <gretl/lib/gretl_panel.h>
#include <gretl/lib/libset.h>
#include <gretl/lib/compat.h>
#include <gretl/lib/missing_private.h>
#include <gretl/lib/estim_private.h>
#include <gretl/lib/system.h>
#include <gretl/lib/tsls.h>
#include <gretl/lib/nls.h>

#if defined(_OPENMP)
 #include <omp.h>
#endif

#include <glib.h>

/**
 * SECTION:estimate
 * @short_description: estimation of regression models
 * @title: Estimation
 * @include: gretl/libgretl.h
 *
 * Most libgretl functions that estimate regression models are 
 * collected here.
 *
 * Please note that most of these functions return a #MODEL
 * struct -- literally a struct, not a pointer to one.
 * To handle such a return value in C you should either (a)
 * declare a MODEL and assign to it, or (b) allocate a MODEL
 * "on the heap" and then assign to its content using the
 * indirection operator. The code fragment below illustrates
 * both approaches.
 *
 * <informalexample><programlisting>
 * #include &lt;gretl/libgretl.h&gt;
 * 
 * int ols_using_gretl (const int *list, DATASET *dset,
 *                      PRN *prn)
 * {
 *     MODEL model;
 *     MODEL *pmod;
 *     int err;
 *
 *     // method (a)
 *     model = lsq(list, dset, OLS, OPT_NONE);
 *     err = model.errcode;
 *     if (!err) {
 *         printmodel(&amp;model, dset, OPT_NONE, prn);
 *     }
 *     clear_model(&amp;model);
 *
 *     // method (b)
 *     pmod = gretl_model_new();
 *     *pmod = lsq(list, dset, OLS, OPT_NONE);
 *     err = pmod->errcode;
 *     if (!err) {
 *         printmodel(pmod, dset, OPT_NONE, prn);
 *     }
 *     gretl_model_free(pmod);
 *
 *     return err;
 * }
 * </programlisting></informalexample>
 * 
 */

/* Comment on 'TINY': It's the minimum value for 'test' (see below)
   that libgretl's Cholesky decomposition routine will accept before
   rejecting a data matrix as too highly collinear.  If you set it too
   high, data sets for which Cholesky could produce reasonable
   estimates will be rejected.  If you set it too low (and 100 *
   DBL_EPSILON is definitely too low), gretl will produce more or less
   worthless coefficient estimates when given highly collinear data.
   Before making a permanent change to the value of TINY, check how
   gretl does on the NIST reference data sets for linear regression
   and ensure you're not getting any garbage results.  The current
   enables us to get decent results on the NIST nonlinear regression
   test suite; it might be a bit too low for some contexts.
*/

#define TINY      8.0e-09 /* was 2.1e-09 (last changed 2007/01/20) */
#define SMALL     1.0e-08 /* threshold for printing a warning for collinearity */
#define YBARZERO  0.5e-14 /* threshold for treating mean of dependent
			     variable as effectively zero */
#define ESSZERO   1.0e-22 /* threshold for considering a tiny error-sum-of-
			     squares value to be effectively zero */

#define XPX_DEBUG 0

static void regress (MODEL *pmod, double *xpy, 
		     double ysum, double ypy, 
		     const DATASET *dset, 
		     double rho, gretlopt opt);
static int hatvar (MODEL *pmod, int n, const DATASET *dset);
static void omitzero (MODEL *pmod, const DATASET *dset,
		      gretlopt opt);
static int lagdepvar (const int *list, const DATASET *dset); 
static int jackknife_vcv (MODEL *pmod, const DATASET *dset);
static double estimate_rho (const int *list, DATASET *dset,
			    gretlopt opt, PRN *prn, int *err);

/* compute statistics for the dependent variable in a model */

static void model_depvar_stats (MODEL *pmod, DATASET *dset)
{
    double xx, sum = 0.0;
    int yno = pmod->list[1];
    int t, dwt = 0;

    if (pmod->ci == WLS && gretl_model_get_int(pmod, "wt_dummy")) {
	dwt = pmod->nwt;
    }

    pmod->ybar = pmod->sdy = NADBL;

    if (pmod->nobs <= 0) {
	return;
    }

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (dwt && dset->Z[pmod->nwt][t] == 0.0) {
	    continue;
	}
	if (!model_missing(pmod, t)) {
	    sum += dset->Z[yno][t];
	} 
    }

    pmod->ybar = sum / pmod->nobs;

    sum = 0.0;
    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (dwt && dset->Z[pmod->nwt][t] == 0.0) {
	    continue;
	}	
	if (!model_missing(pmod, t)) {
	    sum += (dset->Z[yno][t] - pmod->ybar); 
	}
    }

    pmod->ybar = pmod->ybar + sum / pmod->nobs;

    if (fabs(pmod->ybar) < YBARZERO) {
	pmod->ybar = 0.0;
    }

    sum = 0.0;
    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (dwt && dset->Z[pmod->nwt][t] == 0.0) {
	    continue;
	}
	if (!model_missing(pmod, t)) {
	    xx = dset->Z[yno][t] - pmod->ybar;
	    sum += xx * xx;
	}
    }

    sum = (pmod->nobs > 1)? sum / (pmod->nobs - 1) : 0.0;

    pmod->sdy = (sum >= 0)? sqrt(sum) : NADBL;
}

/* determine the degrees of freedom for a model */

static int get_model_df (MODEL *pmod)
{
    int err = 0;

    pmod->ncoeff = pmod->list[0] - 1;
    pmod->dfd = pmod->nobs - pmod->ncoeff;

    if (pmod->dfd < 0) {
	pmod->errcode = E_DF;
        gretl_errmsg_sprintf(_("No. of obs (%d) is less than no. "
			       "of parameters (%d)"), pmod->nobs, 
			     pmod->ncoeff);
	err = 1;
    } else {
	pmod->dfn = pmod->ncoeff - pmod->ifc;
    }

    return err;
}

#define LDDEBUG 0

static int transcribe_ld_vcv (MODEL *targ, MODEL *src)
{
    int nv = targ->ncoeff;
    int nxpx = (nv * nv + nv) / 2;
    int i, j, err = 0;

    err = makevcv(src, src->sigma);

    if (!err && targ->vcv == NULL) {
	targ->vcv = malloc(nxpx * sizeof *targ->vcv);
	if (targ->vcv == NULL) {
	    err = E_ALLOC;
	}
    }

    if (!err) {
	for (i=0; i<nv; i++) {
	    for (j=i; j<nv; j++) {
		targ->vcv[ijton(i, j, nv)] = 
		    src->vcv[ijton(i, j, src->ncoeff)];
	    }
	}
    }

    return err;
}

/* Calculate consistent standard errors (and VCV matrix) when doing
   AR(1) estimation of a model with lagged dependent variable.  See
   Ramanathan, Introductory Econometrics, 5e, p. 450.
*/

static int 
ldepvar_std_errors (MODEL *pmod, DATASET *dset)
{
    MODEL emod;
    const double *x;
    int orig_t1 = dset->t1;
    int orig_t2 = dset->t2;
    double rho = gretl_model_get_double(pmod, "rho_in");
    int origv = dset->v;
    int vnew = pmod->list[0] + 1 - pmod->ifc;
    int *list;
    int vi, vm;
    int i, t;
    int err = 0;

#if LDDEBUG
    PRN *prn = gretl_print_new(GRETL_PRINT_STDOUT, NULL);
    printlist(pmod->list, "pmod->list");
    printf("vnew = %d\n", vnew);
    printf("rho = %g\n", rho);
#endif

    list = gretl_list_new(vnew + pmod->ifc);
    if (list == NULL) {
	pmod->errcode = E_ALLOC;
	return 1;
    }

    err = dataset_add_series(dset, vnew);
    if (err) {
	free(list);
	pmod->errcode = E_ALLOC;
	return 1;
    }

    vi = origv;

    /* dependent var is residual from original model */
    for (t=0; t<dset->n; t++) {
	dset->Z[vi][t] = pmod->uhat[t];
    }    
    strcpy(dset->varname[vi], "eps");
    list[1] = vi++;

    /* indep vars are rho-differenced vars from original model */
    for (i=2; i<=pmod->list[0]; i++) {
	vm = pmod->list[i];
	if (vm == 0) {
	    list[i] = 0;
	    continue;
	}
	sprintf(dset->varname[vi], "%.6s_r", dset->varname[vm]);
	x = dset->Z[vm];
	for (t=0; t<dset->n; t++) {
	    if (t == 0 || na(x[t]) || na(x[t-1])) {
		dset->Z[vi][t] = NADBL;
	    } else {
		dset->Z[vi][t] = x[t] - rho * x[t-1];
	    }
	}
	list[i] = vi++;
    }

    /* last indep var is lagged u-hat */
    for (t=0; t<dset->n; t++) {
	if (t == 0) {
	    dset->Z[vi][t] = NADBL;
	} else { 
	    dset->Z[vi][t] = dset->Z[pmod->list[1]][t-1];
	}
	if (na(dset->Z[vi][t])) {
	    continue;
	}
	for (i=0; i<pmod->ncoeff; i++) {
	    x = dset->Z[pmod->list[i+2]];
	    if (na(x[t-1])) {
		dset->Z[vi][t] = NADBL;
		break;
	    } else {
		dset->Z[vi][t] -= pmod->coeff[i] * x[t-1];
	    }
	}
    }

    list[list[0]] = vi;
    strcpy(dset->varname[vi], "uhat_1");

    dset->t1 = pmod->t1;
    dset->t2 = pmod->t2;

    emod = lsq(list, dset, OLS, OPT_A);
    if (emod.errcode) {
	err = emod.errcode;
    } else {
#if LDDEBUG
	printmodel(&emod, dset, OPT_NONE, prn);
	gretl_print_destroy(prn);
#endif
	for (i=0; i<pmod->ncoeff; i++) {
	    pmod->sderr[i] = emod.sderr[i];
	}

	err = transcribe_ld_vcv(pmod, &emod);
    }
    
    clear_model(&emod);
    
    free(list);
    dataset_drop_last_variables(dset, vnew);

    dset->t1 = orig_t1;
    dset->t2 = orig_t2;

    if (err && !pmod->errcode) {
	pmod->errcode = err;
    }

    return err;
}

/* special computation of statistics for autoregressive models */

static int compute_ar_stats (MODEL *pmod, const DATASET *dset, 
			     double rho)
{
    int i, v, t, yno = pmod->list[1];
    int pwe = (pmod->opt & OPT_P);
    double x, pw1 = 0.0;

    if (gretl_model_add_arinfo(pmod, 1)) {
	pmod->errcode = E_ALLOC;
	return 1;
    }

    if (pwe) {
	pw1 = sqrt(1.0 - rho * rho);
    }

    pmod->arinfo->arlist[0] = pmod->arinfo->arlist[1] = 1;
    pmod->arinfo->rho[0] = rho;
    gretl_model_set_double(pmod, "rho_in", rho);

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (t == pmod->t1 && pwe) {
	    x = pw1 * dset->Z[yno][t];
	    for (i=pmod->ifc; i<pmod->ncoeff; i++) {
		v = pmod->list[i+2];
		x -= pmod->coeff[i] * pw1 * dset->Z[v][t];
	    }
	    if (pmod->ifc) {
		x -= pw1 * pmod->coeff[0];
	    }
	} else {
	    x = dset->Z[yno][t] - rho*dset->Z[yno][t-1];
	    for (i=0; i<pmod->ncoeff; i++) {
		v = pmod->list[i+2];
		x -= pmod->coeff[i] * (dset->Z[v][t] - rho*dset->Z[v][t-1]);
	    }
	}
	pmod->uhat[t] = x;
	pmod->yhat[t] = dset->Z[yno][t] - x;
    }

    pmod->rsq = gretl_corr_rsq(pmod->t1, pmod->t2, dset->Z[yno], pmod->yhat);

    pmod->adjrsq = 
	1.0 - ((1.0 - pmod->rsq) * (pmod->t2 - pmod->t1) / 
	       (double) pmod->dfd);

    return 0;
}

/* calculation of WLS stats in agreement with GNU R */

static void get_wls_stats (MODEL *pmod, const DATASET *dset)
{
    int dumwt = gretl_model_get_int(pmod, "wt_dummy");
    int t, wobs = pmod->nobs, yno = pmod->list[1];
    double x, dy, wmean = 0.0, wsum = 0.0;

    for (t=pmod->t1; t<=pmod->t2; t++) { 
	if (model_missing(pmod, t)) {
	    continue;
	}
	if (dset->Z[pmod->nwt][t] == 0.0 && !dumwt) {
	    wobs--;
	    pmod->dfd -= 1;
	} else {
	    wmean += dset->Z[pmod->nwt][t] * dset->Z[yno][t];
	    wsum += dset->Z[pmod->nwt][t];
	}
    }

    wmean /= wsum;
    x = 0.0;

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (model_missing(pmod, t) || dset->Z[pmod->nwt][t] == 0.0) {
	    continue;
	}	
	dy = dset->Z[yno][t] - wmean;
	x += dset->Z[pmod->nwt][t] * dy * dy;
    }

    pmod->fstt = ((x - pmod->ess) * pmod->dfd) / (pmod->dfn * pmod->ess);
    pmod->rsq = (1 - (pmod->ess / x));
    pmod->adjrsq = 1 - ((1 - pmod->rsq) * (pmod->nobs - 1)/pmod->dfd);
}

static void fix_wls_values (MODEL *pmod, const DATASET *dset)
{
    int t;

    if (gretl_model_get_int(pmod, "wt_dummy")) {
	for (t=pmod->t1; t<=pmod->t2; t++) {
	    if (dset->Z[pmod->nwt][t] == 0.0) {
		pmod->yhat[t] = pmod->uhat[t] = NADBL;
	    }
	}
    } else {
	double ess_orig = 0.0;
	double sw, sigma_orig;

	for (t=pmod->t1; t<=pmod->t2; t++) {
	    if (model_missing(pmod, t)) {
		continue;
	    }
	    if (dset->Z[pmod->nwt][t] == 0.0) {
		pmod->yhat[t] = pmod->uhat[t] = NADBL;
		pmod->nobs -= 1;
	    } else {
		sw = sqrt(dset->Z[pmod->nwt][t]);
		pmod->yhat[t] /= sw;
		pmod->uhat[t] /= sw;
		ess_orig += pmod->uhat[t] * pmod->uhat[t];
	    }
	}

	sigma_orig = sqrt(ess_orig / pmod->dfd);
	gretl_model_set_double(pmod, "ess_orig", ess_orig);
	gretl_model_set_double(pmod, "sigma_orig", sigma_orig);
    }
}

/* drop the weight var from the list of regressors (WLS) */

static void dropwt (int *list)
{
    int i;

    list[0] -= 1;
    for (i=1; i<=list[0]; i++) {
	list[i] = list[i+1];
    }
}

static int model_missval_count (const MODEL *pmod)
{
    int mc = 0;

    if (pmod->missmask != NULL) {
	int t;

	for (t=pmod->t1; t<=pmod->t2; t++) {
	    if (pmod->missmask[t] == '1') {
		mc++;
	    }
	}
    }

    return mc;
}

#define SMPL_DEBUG 0

static int 
lsq_check_for_missing_obs (MODEL *pmod, gretlopt opts, DATASET *dset, 
			   int *misst)
{
    int ref_mask = reference_missmask_present();
    int missv = 0;
    int reject_missing = 0;

#if SMPL_DEBUG
    fprintf(stderr, "lsq_check_for_missing_obs: ref_mask = %d\n",
	    ref_mask);
#endif

    if (ref_mask) {
	int err = apply_reference_missmask(pmod);

	/* If there was a reference mask present, it was put there
	   as part of a hypothesis test on some original model, and
	   it should be respected in estimation of this model */

	if (err) {
	    pmod->errcode = E_ALLOC;
	    return 1;
	} else {
	    return 0;
	}
    } 

    /* can't do HAC VCV with missing obs in middle */
    if ((opts & OPT_R) && dataset_is_time_series(dset) &&
	!libset_get_bool(FORCE_HC)) {
	reject_missing = 1;
    } 

    if (opts & OPT_M) {
	reject_missing = 1;
    }

    if (reject_missing) {
	/* reject missing obs within adjusted sample */
	missv = model_adjust_sample(pmod, dset->n, 
				    (const double **) dset->Z, 
				    misst);
    } else {
	/* we'll try to work around missing obs */
	missv = model_adjust_sample(pmod, dset->n, 
				    (const double **) dset->Z, 
				    NULL);
    }

#if SMPL_DEBUG
    if (1) {
	char t1s[OBSLEN], t2s[OBSLEN];
	int misscount = model_missval_count(pmod);

	ntodate(t1s, pmod->t1, dset);
	ntodate(t2s, pmod->t2, dset);
	fprintf(stderr, "*** after adjustment, t1=%d (%s), t2=%d (%s)\n", 
		pmod->t1, t1s, pmod->t2, t2s);
	fprintf(stderr, "Valid observations in range = %d\n", 
		pmod->t2 - pmod->t1 + 1 - misscount);
    }
#endif

    return missv;
}

static int
lagged_depvar_check (MODEL *pmod, const DATASET *dset)
{
    int ldv = lagdepvar(pmod->list, dset);

    if (ldv) {
	gretl_model_set_int(pmod, "ldepvar", ldv);
    } else if (gretl_model_get_int(pmod, "ldepvar")) {
	gretl_model_destroy_data_item(pmod, "ldepvar");
    }

    return ldv;
}

static void 
log_depvar_ll (MODEL *pmod, const DATASET *dset)
{
    char parent[VNAMELEN];

    if (series_is_log(dset, pmod->list[1], parent)) {
	double jll = pmod->lnL;
	int t;

	for (t=0; t<dset->n; t++) {
	    if (!na(pmod->uhat[t])) {
		jll -= dset->Z[pmod->list[1]][t];
	    }
	}
	gretl_model_set_double(pmod, "jll", jll);
	gretl_model_set_string_as_data(pmod, 
				       "log-parent", 
				       gretl_strdup(parent));
    }
}

static int check_weight_var (MODEL *pmod, const double *w, int *effobs)
{
    const char *wtzero = 
	N_("Weight variable is all zeros, aborting regression");
    const char *wtneg = 
	N_("Weight variable contains negative values");
    int t, allzero = 1;

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (w[t] < 0.0) {
	    gretl_errmsg_set(_(wtneg));
	    pmod->errcode = E_DATA;
	    return 1;
	} else if (w[t] > 0.0) {
	    allzero = 0;
	}
    }

    if (allzero) {
	gretl_errmsg_set(_(wtzero));
	pmod->errcode = E_DATA;
	return 1;
    }	

    *effobs = gretl_isdummy(pmod->t1, pmod->t2, w);

    if (*effobs) {
	/* the weight var is a dummy, with effobs 1s */
	gretl_model_set_int(pmod, "wt_dummy", 1);
    }

    return 0;
}

void maybe_shift_ldepvar (MODEL *pmod, DATASET *dset)
{
    if (gretl_model_get_int(pmod, "ldepvar")) {
	lagged_depvar_check(pmod, dset);
    }
}

/*
 * XTX_XTy:
 * @list: list of variables in model.
 * @t1: starting observation.
 * @t2: ending observation.
 * @dset: pointer to dataset.
 * @nwt: ID number of variable used as weight, or 0.
 * @rho: quasi-differencing coefficent, or 0.0;
 * @pwe: if non-zero, use Prais-Winsten for first observation.
 * @xpx: on output, X'X matrix as lower triangle, stacked by columns.
 * @xpy: on output, X'y vector (but see below).
 * @ysum: location to recieve \sum y_i, or NULL.
 * @ypy: location to recieve (scalar) y'y, or NULL.
 * @mask: missing observations mask, or NULL.
 *
 * Calculates X'X and X'y, with various possible transformations
 * of the original data, depending on @nwt, @rho and @pwe.
 * If X'y is not required, @xpy can be given as NULL.
 *
 * Note: the y-related pointer arguments @xpy, @ysum, and @ypy 
 * form a "package": either all should be given, or all should 
 * be NULL.
 *
 * Returns: 0 on success, non-zero on error.
 */

static int XTX_XTy (const int *list, int t1, int t2, 
		    const DATASET *dset, int nwt, 
		    double rho, int pwe,
		    double *xpx, double *xpy, 
		    double *ysum, double *ypy,
		    const char *mask)
{
    int yno = list[1];
    int lmin = (xpy != NULL)? 2 : 1;
    int lmax = list[0];
    int qdiff = (rho != 0.0);
    const double *y = NULL;
    const double *w = NULL;
    const double *xi = NULL;
    const double *xj = NULL;
    double x, pw1;
    int i, j, t, m;

    /* Prais-Winsten term */
    if (qdiff && pwe) {
	pw1 = sqrt(1.0 - rho * rho);
    } else {
	pwe = 0;
	pw1 = 0.0;
    }

    /* dependent variable */
    y = dset->Z[yno];

    if (nwt) {
	/* weight variable */
	w = dset->Z[nwt];
    }

    if (xpy != NULL) {
	*ysum = *ypy = 0.0;

	for (t=t1; t<=t2; t++) {
	    if (masked(mask, t)) {
		continue;
	    }
	    x = y[t]; 
	    if (qdiff) {
		if (pwe && t == t1) {
		    x = pw1 * y[t];
		} else {
		    x -= rho * y[t-1];
		}
	    } else if (nwt) {
		x *= sqrt(w[t]);
	    }
	    *ysum += x;
	    *ypy += x * x;
	}

	if (*ypy <= 0.0) {
	    /* error condition */
	    return yno; 
	} 
    }   

    m = 0;

    if (qdiff) {
	/* quasi-difference the data */
	for (i=lmin; i<=lmax; i++) {
	    xi = dset->Z[list[i]];
	    for (j=i; j<=lmax; j++) {
		xj = dset->Z[list[j]];
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    if (pwe && t == t1) {
			x += pw1 * xi[t1] * pw1 * xj[t];
		    } else {
			x += (xi[t] - rho * xi[t-1]) * 
			    (xj[t] - rho * xj[t-1]);
		    }
		}
		if (i == j && x < DBL_EPSILON)  {
		    return E_SINGULAR;
		}
		xpx[m++] = x;
	    }
	    if (xpy != NULL) {
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    if (pwe && t == t1) {
			x += pw1 * y[t] * pw1 * xi[t];
		    } else {
			x += (y[t] - rho * y[t-1]) *
			    (xi[t] - rho * xi[t-1]);
		    }
		}
		xpy[i-2] = x;
	    }
	}
    } else if (nwt) {
	/* weight the data */
	for (i=lmin; i<=lmax; i++) {
	    xi = dset->Z[list[i]];
	    for (j=i; j<=lmax; j++) {
		xj = dset->Z[list[j]];
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    if (!masked(mask, t)) {
			x += w[t] * xi[t] * xj[t];
		    }
		}
		if (i == j && x < DBL_EPSILON) {
		    return E_SINGULAR;
		}   
		xpx[m++] = x;
	    }
	    if (xpy != NULL) {
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    if (!masked(mask, t)) {
			x += w[t] * y[t] * xi[t];
		    }
		}
		xpy[i-2] = x;
	    }
	}
    } else if (mask == NULL) {
	/* plain data, no missing obs mask */
#if defined(_OPENMP)
	int k = lmax - lmin + 1;
	guint64 T = t2 - t1 + 1;
	guint64 fpm = T * (k*k + k)/2;
	int vi, err = 0;

	if (!libset_use_openmp(fpm)) {
	    goto st_mode;
	}
#pragma omp parallel for private(i, j, t, vi, x)
	for (i=lmin; i<=lmax; i++) {
	    vi = list[i];
	    for (j=i; j<=lmax; j++) {
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    x += dset->Z[vi][t] * dset->Z[list[j]][t];
		}
		if (i == j && x < DBL_EPSILON) {
		    err = E_SINGULAR;
		}
		xpx[ijton(i-lmin, j-lmin, k)] = x;
	    }
	    if (xpy != NULL) {
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    x += y[t] * dset->Z[vi][t];
		}
		xpy[i-2] = x;
	    }
	}
	return err;
    
    st_mode:
#endif
	for (i=lmin; i<=lmax; i++) {
	    xi = dset->Z[list[i]];
	    for (j=i; j<=lmax; j++) {
		xj = dset->Z[list[j]];
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    x += xi[t] * xj[t];
		}
		if (i == j && x < DBL_EPSILON) {
		    return E_SINGULAR;
		}
		xpx[m++] = x;
	    }
	    if (xpy != NULL) {
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    x += y[t] * xi[t];
		}
		xpy[i-2] = x;
	    }
	}
    } else {
	/* plain data, but with missing obs mask */
	for (i=lmin; i<=lmax; i++) {
	    xi = dset->Z[list[i]];
	    for (j=i; j<=lmax; j++) {
		xj = dset->Z[list[j]];
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    if (!masked(mask, t)) {
			x += xi[t] * xj[t];
		    }
		}
		if (i == j && x < DBL_EPSILON) {
		    return E_SINGULAR;
		}
		xpx[m++] = x;
	    }
	    if (xpy != NULL) {
		x = 0.0;
		for (t=t1; t<=t2; t++) {
		    if (!masked(mask, t)) {
			x += y[t] * xi[t];
		    }
		}
		xpy[i-2] = x;
	    }
	}
    }

    return 0; 
}

/**
 * gretl_XTX:
 * @pmod: reference model.
 * @dset: dataset struct.
 * @err: location to receive error code.
 *
 * (Re-)calculates X'X, with various possible transformations
 * of the original data depending on whether estimation of 
 * @pmod involved weighting or quasi-differencing.
 *
 * Returns: The vech of X'X or NULL on error.
 */

double *gretl_XTX (const MODEL *pmod, const DATASET *dset, int *err)
{
    int *xlist;
    double *xpx;
    double rho;
    int pwe = 0;
    int k, m;
    
    *err = 0;

    xlist = gretl_model_get_x_list(pmod);
    if (xlist == NULL) {
	*err = E_DATA;
	return NULL;
    }

    k = xlist[0];
    m = k * (k + 1) / 2;

    xpx = malloc(m * sizeof *xpx);
    if (xpx == NULL) {
	*err = E_ALLOC;
	free(xlist);
	return NULL;
    }

    if (pmod->ci == AR1 && (pmod->opt & OPT_P)) {
	pwe = 1;
    }

    rho = gretl_model_get_double(pmod, "rho_in");
    if (na(rho)) {
	rho = 0.0;
    }

    *err = XTX_XTy(xlist, pmod->t1, pmod->t2, dset, 
		   pmod->nwt, rho, pwe, xpx, 
		   NULL, NULL, NULL, 
		   pmod->missmask);

    free(xlist);

    return xpx;
}

static int gretl_choleski_regress (MODEL *pmod, const DATASET *dset,
				   double rho, int pwe, gretlopt opt)
{
    double ysum = 0.0, ypy = 0.0;
    double *xpy;
    int k = pmod->ncoeff;
    int nxpx = k * (k + 1) / 2;
    int i;

    if (nxpx == 0) {
	fprintf(stderr, "problem: nxpx = 0\n");
	pmod->errcode = E_DATA;
	return pmod->errcode;
    }

    xpy = malloc(k * sizeof *xpy);
    if (xpy == NULL) {
	pmod->errcode = E_ALLOC;
	return pmod->errcode;
    }

    pmod->xpx = malloc(nxpx * sizeof *pmod->xpx);
    pmod->coeff = malloc(k * sizeof *pmod->coeff);
    pmod->sderr = malloc(k * sizeof *pmod->sderr);

    if (pmod->yhat == NULL) {
	pmod->yhat = malloc(pmod->full_n * sizeof *pmod->yhat);
    } 

    if (pmod->uhat == NULL) {
	pmod->uhat = malloc(pmod->full_n * sizeof *pmod->uhat);
    }

    if (pmod->xpx == NULL || pmod->coeff == NULL ||
	pmod->sderr == NULL || pmod->yhat == NULL || 
	pmod->uhat == NULL) {
	free(xpy);
	pmod->errcode = E_ALLOC;
	return pmod->errcode;
    }

    for (i=0; i<k; i++) {
	xpy[i] = 0.0;
    }
    for (i=0; i<nxpx; i++) {
	pmod->xpx[i] = 0.0;
    }

    /* calculate regression results, Cholesky style */
    pmod->errcode = XTX_XTy(pmod->list, pmod->t1, pmod->t2, dset, 
			    pmod->nwt, rho, pwe, pmod->xpx, xpy, 
			    &ysum, &ypy, pmod->missmask);

#if XPX_DEBUG
    for (i=0; i<k; i++) {
	fprintf(stderr, "xpy[%d] = %g\n", i, xpy[i]);
    }
    for (i=0; i<nxpx; i++) {
	fprintf(stderr, "xpx[%d] = %g\n", i, pmod->xpx[i]);
    }
    fputc('\n', stderr);
#endif

    if (!pmod->errcode) {
	regress(pmod, xpy, ysum, ypy, dset, rho, opt);
    }

    free(xpy);

    return pmod->errcode;
}

static int gretl_null_regress (MODEL *pmod, const DATASET *dset)
{
    double yt;
    int t;

    if (pmod->yhat == NULL) {
	pmod->yhat = malloc(pmod->full_n * sizeof *pmod->yhat);
    } 

    if (pmod->uhat == NULL) {
	pmod->uhat = malloc(pmod->full_n * sizeof *pmod->uhat);
    }

    if (pmod->yhat == NULL || pmod->uhat == NULL) {
	pmod->errcode = E_ALLOC;
	return pmod->errcode;
    }

    pmod->nobs = 0;
    pmod->ifc = 0;
    pmod->ess = 0.0;
    pmod->rsq = pmod->adjrsq = 0.0;

    for (t=0; t<pmod->full_n; t++) {
	yt = dset->Z[pmod->list[1]][t];
	if (t < pmod->t1 || t > pmod->t2 || na(yt)) {
	    pmod->uhat[t] = pmod->yhat[t] = NADBL;
	} else {
	    pmod->uhat[t] = yt;
	    pmod->yhat[t] = 0.0;
	    pmod->ess += yt * yt;
	    pmod->nobs += 1;
	}
    }

    if (pmod->ess == 0) {
	pmod->sigma = 0.0;
    } else if (pmod->nobs > 1) {
	pmod->sigma = sqrt(pmod->ess / (pmod->nobs - 1));
    } else {
	pmod->errcode = E_DATA;
    }

    return pmod->errcode;
}

/* check whether the variable with ID @yno is all zeros;
   return 1 if so, otherwise return 0 */

static int depvar_zero (const MODEL *pmod, int yno, const double **Z)
{
    double yt;
    int t, ret = 1;

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (model_missing(pmod, t)) {
	    continue;
	}
	yt = Z[yno][t];
	if (pmod->nwt) {
	    yt *= Z[pmod->nwt][t];
	}
	if (yt != 0.0) {
	    ret = 0;
	    break;
	}
    }

    return ret;
}

/* limited freeing of elements before passing a model
   on for QR estimation in the case of (near-)singularity 
*/

static void model_free_storage (MODEL *pmod)
{
    free(pmod->xpx);
    free(pmod->coeff);
    free(pmod->sderr);

    pmod->xpx = NULL;
    pmod->coeff = NULL;
    pmod->sderr = NULL;
}

/*
 * ar1_lsq:
 * @list: model specification.
 * @dset: dataset struct.
 * @ci: command index, e.g. OLS, AR1.
 * @opt: option flags (see lsq and ar1_model).
 * @rho: coefficient for quasi-differencing the data, or 0.
 *
 * Nests lsq() and ar1_model(); is also used internally with ci == OLS
 * but rho != 0.0 when estimating rho via, e.g., Cochrane-Orcutt
 * iteration.
 *
 * Returns: model struct containing the estimates.
 */

static MODEL ar1_lsq (const int *list, DATASET *dset, 
		      GretlCmdIndex ci, gretlopt opt, 
		      double rho)
{
    MODEL mdl;
    int effobs = 0;
    int missv = 0, misst = 0;
    int jackknife = 0;
    int pwe = (opt & OPT_P);
    int nullmod = 0, ldv = 0;
    int yno, i;

    gretl_model_init(&mdl, dset);

    if (list == NULL || dset == NULL || dset->Z == NULL) {
	fprintf(stderr, "E_DATA: lsq: list = %p, dset = %p\n",
		(void *) list, (void *) dset);
	mdl.errcode = E_DATA;
        return mdl;
    }

    if (opt & OPT_C) {
	/* cluster option implies robust */
	opt |= OPT_R;
    }  

    if (ci == AR1) {
	if (opt & OPT_P) {
	    /* OPT_P: Prais-Winsten */
	    mdl.opt |= OPT_P;
	} else if (opt & OPT_H) {
	    /* OPT_H: Hildreth-Lu */
	    mdl.opt |= OPT_H;
	}
    } 

    if (list[0] == 1 && ci == OLS && (opt & OPT_U)) {
	/* null model OK */
	nullmod = 1;
    } else if (list[0] == 1 || dset->v == 1) {
	fprintf(stderr, "E_DATA: lsq: list[0] = %d, dset->v = %d\n",
		list[0], dset->v);
	mdl.errcode = E_DATA;
        return mdl;
    }

    /* preserve a copy of the list supplied, for future reference */
    mdl.list = gretl_list_copy(list);
    if (mdl.list == NULL) {
        mdl.errcode = E_ALLOC;
        return mdl;
    }

    mdl.t1 = dset->t1;
    mdl.t2 = dset->t2;
    mdl.full_n = dset->n;
    mdl.ci = ci;

    /* Doing weighted least squares? */
    if (ci == WLS) { 
	check_weight_var(&mdl, dset->Z[mdl.list[1]], &effobs);
	if (mdl.errcode) {
	    return mdl;
	}
	mdl.nwt = mdl.list[1];
    } else {
	mdl.nwt = 0;
    }

    /* sanity check */
    if (mdl.t1 < 0 || mdl.t2 > dset->n - 1) {
        mdl.errcode = E_DATA;
        goto lsq_abort;
    }

    /* adjust sample range and check for missing obs: this
       may set the model errcode */
    missv = lsq_check_for_missing_obs(&mdl, opt, dset, &misst);
    if (mdl.errcode) {
        goto lsq_abort;
    }

    /* react to presence of unhandled missing obs */
    if (missv) {
	if (dated_daily_data(dset)) {
	    if (repack_missing_daily_obs(&mdl, dset)) {
		return mdl;
	    }
	} else {
	    gretl_errmsg_sprintf(_("Missing value encountered for "
				   "variable %d, obs %d"), 
				 missv, misst);
	    mdl.errcode = E_DATA;
	    return mdl;
	} 
    }

    if (ci == WLS) {
	dropwt(mdl.list);
    }

    yno = mdl.list[1];
    
    /* check for unknown vars in list */
    for (i=1; i<=mdl.list[0]; i++) {
        if (mdl.list[i] > dset->v - 1) {
            mdl.errcode = E_UNKVAR;
            goto lsq_abort;
        }
    } 

    /* check for zero dependent var */
    if (depvar_zero(&mdl, yno, (const double **) dset->Z)) {  
        mdl.errcode = E_ZERO;
        goto lsq_abort; 
    } 

    /* drop any vars that are all zero and repack the list */
    omitzero(&mdl, dset, opt);

    /* if regressor list contains a constant, record this fact and 
       place it first among the regressors */
    mdl.ifc = reglist_check_for_const(mdl.list, dset);

    /* Check for presence of lagged dependent variable? 
       (Don't bother if this is an auxiliary regression.) */
    if (!(opt & OPT_A)) {
	ldv = lagged_depvar_check(&mdl, dset);
    }

    /* AR1: advance the starting observation by one? */
    if (rho != 0.0 && !pwe) {
	mdl.t1 += 1;
    }

    mdl.ncoeff = mdl.list[0] - 1; 
    if (effobs) {
	mdl.nobs = effobs; /* FIXME? */
    } else {
	mdl.nobs = mdl.t2 - mdl.t1 + 1;
	if (mdl.missmask != NULL) {
	    mdl.nobs -= model_missval_count(&mdl);
	}
    }

    /* check degrees of freedom */
    if (get_model_df(&mdl)) {
        goto lsq_abort; 
    }

    /* if df correction is not wanted, record this fact */
    if (opt & OPT_N) {
	mdl.opt |= OPT_N;
    }

    if (dataset_is_time_series(dset)) {
	opt |= OPT_T;
    } 

    /* remove Durbin-Watson p-value flag if not appropriate */
    if (ldv || !(opt & OPT_T)) {
	opt &= ~OPT_I;
    }

    if ((opt & OPT_J) || ((opt & OPT_R) && libset_get_int(HC_VERSION) == 4)) {
	jackknife = 1;
    }

     if (nullmod) {
	gretl_null_regress(&mdl, dset);
    } else if (!jackknife && (opt & (OPT_R | OPT_I))) { 
	mdl.rho = rho;
	gretl_qr_regress(&mdl, dset, opt);
    } else {
	gretl_choleski_regress(&mdl, dset, rho, pwe, opt);
	if (mdl.errcode == E_SINGULAR && !jackknife) {
	    /* near-perfect collinearity is better handled by QR */
	    model_free_storage(&mdl);
	    mdl.rho = rho;
	    gretl_qr_regress(&mdl, dset, opt);
	}
    }

    if (mdl.errcode) {
	goto lsq_abort;
    }

    /* get the mean and sd of dep. var. and make available */
    model_depvar_stats(&mdl, dset);

    /* Doing an autoregressive procedure? */
    if (ci == AR1) {
	if (compute_ar_stats(&mdl, dset, rho)) { 
	    goto lsq_abort;
	}
	if (ldv) {
	    ldepvar_std_errors(&mdl, dset);
	    if (mdl.errcode) {
		goto lsq_abort;
	    }
	}
	if ((opt & OPT_H) && (opt & OPT_B)) {
	    gretl_model_set_int(&mdl, "no-corc", 1);
	}
    }

    /* weighted least squares: fix yhat and uhat; add calculation of
       ESS and sigma based on unweighted data
    */
    if (ci == WLS) {
	get_wls_stats(&mdl, dset);
	fix_wls_values(&mdl, dset);
    }

    if (mdl.missmask == NULL && (opt & OPT_T) && !(opt & OPT_I)) {
	mdl.rho = rhohat(1, mdl.t1, mdl.t2, mdl.uhat);
	mdl.dw = dwstat(1, &mdl, dset);
    } else if (!(opt & OPT_I)) {
	mdl.rho = mdl.dw = NADBL;
    }

    /* weird special case: degenerate model */
    if (mdl.ncoeff == 1 && mdl.ifc) {
	mdl.rsq = mdl.adjrsq = 0.0;
	mdl.fstt = NADBL;
    }

    /* Generate model selection statistics */
    if (ci == AR1) {
	mdl.lnL = NADBL;
	mle_criteria(&mdl, 0);
    } else {
	ls_criteria(&mdl);
    }
    if (!(opt & OPT_A) && !na(mdl.lnL)) {
	log_depvar_ll(&mdl, dset);
    }

    /* HCCME version HC3a */
    if (jackknife) {
	mdl.errcode = jackknife_vcv(&mdl, dset);
    }

 lsq_abort:

    /* If we reshuffled any missing observations, put them
       back in their right places now */
    if (gretl_model_get_int(&mdl, "daily_repack")) {
	undo_daily_repack(&mdl, dset);
    }

    if (!(opt & OPT_A)) {
	/* if it's not an auxiliary regression, set an ID number
	   on the model */
	set_model_id(&mdl);
    }

    return mdl;
}

/**
 * lsq:
 * @list: dependent variable plus list of regressors.
 * @dset: dataset struct.
 * @ci: one of the command indices in #LSQ_MODEL.
 * @opt: option flags: zero or more of the following --
 *   OPT_R: compute robust standard errors;
 *   OPT_A: treat as auxiliary regression (don't bother checking
 *     for presence of lagged dependent var, don't augment model count);
 *   OPT_N: don't use degrees of freedom correction for standard
 *      error of regression;
 *   OPT_M: reject missing observations within sample range;
 *   OPT_Z: (internal use) suppress the automatic elimination of 
 *      perfectly collinear variables.
 *   OPT_X: compute "variance matrix" as just (X'X)^{-1}
 *   OPT_B: don't compute R^2.
 *   OPT_I: compute Durbin-Watson p-value.
 *   OPT_U: treat null model as OK.
 *   OPT_P: (ar1 only): use Prais-Winsten.
 *
 * Computes least squares estimates of the model specified by @list,
 * using an estimator determined by the value of @ci.
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL lsq (const int *list, DATASET *dset, GretlCmdIndex ci, 
	   gretlopt opt)
{
    return ar1_lsq(list, dset, ci, opt, 0.0);
}

/**
 * ar1_model:
 * @list: model specification.
 * @dset: dataset struct.
 * @opt: option flags: may include OPT_H to use Hildreth-Lu,
 *       OPT_P to use Prais-Winsten, OPT_B to suppress Cochrane-Orcutt
 *       fine-tuning of Hildreth-Lu results, OPT_G to generate
 *       a gnuplot graph of the search in Hildreth-Lu case.
 * @prn: gretl printer.
 *
 * Returns: model struct containing the estimates.
 */

MODEL ar1_model (const int *list, DATASET *dset, 
		 gretlopt opt, PRN *prn)
{
    MODEL mdl;
    double rho;
    int err = 0;

    rho = estimate_rho(list, dset, opt, prn, &err);

    if (err) {
	mdl.errcode = err;
	return mdl;
    } else {
	return ar1_lsq(list, dset, AR1, opt, rho);
    }
}

static int make_ess (MODEL *pmod, const DATASET *dset)
{
    int i, t, yno = pmod->list[1], l0 = pmod->list[0];
    int nwt = pmod->nwt;
    double yhat, resid;

    pmod->ess = 0.0;

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (nwt && dset->Z[nwt][t] == 0.0) {
	    continue;
	}
	if (model_missing(pmod, t)) {
	    continue;
	}
	yhat = 0.0;
	for (i=2; i<=l0; i++) {
	    yhat += pmod->coeff[i-2] * dset->Z[pmod->list[i]][t];
	}
	resid = dset->Z[yno][t] - yhat;
	if (nwt) {
	    resid *= sqrt(dset->Z[nwt][t]);
	}
	pmod->ess += resid * resid;
    }

    return 0;
}

#define SMALLDIFF 9.0e-16

/* The heuristic used here is that the model effectively
   contains a constant or intercept if the means of y and
   yhat are the same, where "the same" means that the
   relative difference is less than SMALLDIFF.
*/

int check_for_effective_const (MODEL *pmod, const DATASET *dset)
{
    double reldiff, x1 = 0.0, x2 = 0.0;
    int yno = pmod->list[1];
    int t, ret = 0;

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (!na(pmod->yhat[t])) {
	    x1 += pmod->yhat[t];
	    x2 += dset->Z[yno][t];
	}
    }

    reldiff = fabs((x1 - x2) / x2);
#if 0
    fprintf(stderr, "check_for_effective_const: reldiff = %g\n", reldiff);
#endif

    if (reldiff < SMALLDIFF) {
	gretl_model_set_int(pmod, "effconst", 1);
	pmod->dfn -= 1;
	ret = 1;
    } else if (gretl_model_get_int(pmod, "effconst")) {
	gretl_model_set_int(pmod, "effconst", 0);
	pmod->dfn += 1;
    }

    return ret;
}

static void uncentered_r_squared (MODEL *pmod, const DATASET *dset)
{
    double y0 = dset->Z[pmod->list[1]][pmod->t1];

    /* special computation for the case of TSS = 0, i.e.
       the dependent variable is a constant */

    if (y0 > 0) {
	double tss = pmod->nobs * y0 * y0;

	pmod->rsq = 1 - (pmod->ess / tss);
	gretl_model_set_int(pmod, "uncentered", 1);
    }
}

static void compute_r_squared (MODEL *pmod, const DATASET *dset, 
			       int *ifc)
{
    int yno = pmod->list[1];

    pmod->rsq = 1.0 - (pmod->ess / pmod->tss);

    if (*ifc == 0) {
	*ifc = check_for_effective_const(pmod, dset);
    }

    if (pmod->dfd > 0) {
	double yt, den = 0.0;

	if (*ifc) {
	    den = pmod->tss * pmod->dfd;
	    pmod->adjrsq = 1 - (pmod->ess * (pmod->nobs - 1) / den);
	} else {
	    /* model does not contain a constant */
	    int t;

	    for (t=pmod->t1; t<=pmod->t2; t++) {
		if (!na(pmod->yhat[t])) {
		    yt = dset->Z[yno][t];
		    den += yt * yt;
		}
	    }

	    /* make the centered R^2 available for output */
	    gretl_model_set_double(pmod, "centered-R2", pmod->rsq);

	    /* but make the "official" figure the uncentered R^2,
	       as per NIST, R, Stata, SPSS, ... */
	    pmod->rsq = 1 - pmod->ess / den;
	    pmod->adjrsq = 
		1.0 - ((1.0 - pmod->rsq) * (pmod->nobs - 1.0) / pmod->dfd);
	} 
    }

    if (pmod->rsq < 0.0) {
	pmod->rsq = 0.0;
    }
}

/*
  diaginv: solves for the diagonal elements of X'X inverse.

  xpx = Cholesky-decomposed X'X
  tmp = work array, length >= nv
  diag = diagonal elements of {X'X}^{-1} (output)
  nv = number of regression coefficients = length of diag
*/

static void diaginv (const double *xpx, double *tmp, double *diag, 
		     int nv)
{
    int kk, l, m, k, i, j;
    double d, e;

    kk = 0;

    for (l=0; l<nv-1; l++) {
        d = xpx[kk];
        tmp[l] = d;
        e = d * d;
        m = 0;
        if (l > 0) {
	    for (j=1; j<=l; j++) {
		m += nv - j;
	    }
	}
        for (i=l+1; i<nv; i++) {
            d = 0.0;
            k = i + m;
            for (j=l; j<i; j++) {
                d += tmp[j] * xpx[k];
                k += nv - (j+1);
            }
            d = -d * xpx[k];
            tmp[i] = d;
            e += d * d;
        }
	kk += nv - l;
        diag[l] = e;
    }

    diag[nv-1] = xpx[kk] * xpx[kk];
}

/*
  cholbeta: in-place Cholesky decomposition of X'X (pmod->xpx) (lower
  triangular matrix stacked in columns) plus solution for the
  least-squares coefficient estimates.

  pmod->xpx = X'X on input and Cholesky decomposition on output
  xpy = the X'y vector on input and Cholesky-transformed t
        vector on output 
  rss = location to receive regression sum of squares

  The number of floating-point operations is basically 3.5 * nv^2
  plus (nv^3) / 3.
*/

static int cholbeta (MODEL *pmod, double *xpy, double *rss)
{
    int i, j, k, kk, l, jm1;
    double e, d, d1, d2, test, xx;
    double *xpx = pmod->xpx;
    double *coeff = pmod->coeff;
    int nc = pmod->ncoeff;

    if (xpx[0] <= 0.0) {
	fprintf(stderr, "%s %d: xpx <= 0.0\n", __FILE__, __LINE__);
	return E_NAN;
    }

    e = 1.0 / sqrt(xpx[0]);
    xpx[0] = e;
    xpy[0] *= e;
    for (i=1; i<nc; i++) {
	xpx[i] *= e;
    }

    kk = nc;

    for (j=1; j<nc; j++) {
	/* diagonal elements */
        d = d1 = 0.0;
        k = jm1 = j;

        for (l=1; l<=jm1; l++) {
            xx = xpx[k];
            d1 += xx * xpy[l-1];
            d += xx * xx;
            k += nc - l;
        }

        d2 = xpx[kk] - d;
	test = d2 / xpx[kk];

	/* check for singularity */
        if (test < TINY) {
	    fprintf(stderr, "cholbeta: test[%d] = %g\n", j, test);
	    *rss = -1.0;
	    return E_SINGULAR;
        } else if (test < SMALL) {
	    gretl_model_set_int(pmod, "near-singular", 1);
	} 

        e = 1 / sqrt(d2);
        xpx[kk] = e;
        xpy[j] = (xpy[j] - d1) * e;

	/* off-diagonal elements */
        for (i=j+1; i<nc; i++) {
            kk++;
            d = 0.0;
            k = j;
            for (l=1; l<=jm1; l++) {
                d += xpx[k] * xpx[k-j+i];
                k += nc - l;
            }
            xpx[kk] = (xpx[kk] - d) * e;
        }
        kk++;
    }

    kk--;

    /* calculate regression sum of squares */
    d = 0.0;
    for (j=0; j<nc; j++) {
	d += xpy[j] * xpy[j];
    }
    *rss = d;

    /* back-solve for the coefficients */

    coeff[nc-1] = xpy[nc-1] * xpx[kk];

    for (j=nc-2; j>=0; j--) {
	d = xpy[j];
	for (i=nc-1; i>j; i--) {
	    d -= coeff[i] * xpx[--kk];
	}
	coeff[j] = d * xpx[--kk];
    }

    for (j=0; j<nc; j++) {
	if (isnan(coeff[j])) {
	    fprintf(stderr, "%s %d: coeff %d is NaN\n", __FILE__, __LINE__, j);
	    return E_NAN;
	}
    }

    return 0; 
}

/*
  regress: takes X'X (pmod->xpx) and X'y (@xpy) and
  computes OLS estimates and associated statistics.

  n = total number of observations per series in data set
  pmod->ifc = 1 if constant is present in model, else = 0

  ess = error sum of squares
  sigma = standard error of regression
  fstt = F-statistic
  pmod->coeff = array of regression coefficients
  pmod->sderr = corresponding array of standard errors
*/

static void regress (MODEL *pmod, double *xpy, 
		     double ysum, double ypy,
		     const DATASET *dset,
		     double rho, gretlopt opt)
{
    int ifc = pmod->ifc;
    int n = pmod->full_n;
    double zz, rss = 0.0;
    double s2 = 0.0;
    double *diag = NULL;
    int i, err = 0;

    for (i=0; i<n; i++) {
	pmod->yhat[i] = pmod->uhat[i] = NADBL;
    }    

    zz = ysum * ysum / pmod->nobs;
    pmod->tss = ypy - zz;

    /*  Cholesky-decompose X'X and find the coefficients */
    err = cholbeta(pmod, xpy, &rss);
    if (err) {
        pmod->errcode = err;
        return;
    }   
    
    if (rho != 0.0) {
	pmod->ess = ypy - rss;
    } else {
	make_ess(pmod, dset);
	rss = ypy - pmod->ess;
    }

    if (fabs(pmod->ess) < ESSZERO) {
	pmod->ess = 0.0;
    } else if (pmod->ess < 0.0) { 
	gretl_errmsg_sprintf(_("Error sum of squares (%g) is not > 0"),
			     pmod->ess);
        return; 
    }

    if (pmod->dfd == 0) {
	pmod->sigma = 0.0;
	pmod->adjrsq = NADBL;
    } else {
	if (pmod->opt & OPT_N) {
	    /* no-df-corr */
	    s2 = pmod->ess / pmod->nobs;
	} else {
	    s2 = pmod->ess / pmod->dfd;
	}
	pmod->sigma = sqrt(s2);
    }

    if (pmod->tss < DBL_EPSILON) {
	pmod->rsq = pmod->adjrsq = NADBL;
    } 

    hatvar(pmod, n, dset); 
    if (pmod->errcode) return;

    if (!(opt & OPT_B)) {
	/* changed 2008-09-25 */
	if (pmod->tss > 0.0) {
	    compute_r_squared(pmod, dset, &ifc);
	} else if (pmod->tss == 0.0) {
	    uncentered_r_squared(pmod, dset);
	}
    }

    if (s2 <= 0.0 || pmod->dfd == 0 || pmod->dfn == 0) {
	pmod->fstt = NADBL;
    } else if (pmod->rsq == 1.0) {
	pmod->fstt = NADBL;
    } else {
	pmod->fstt = (rss - zz * ifc) / (s2 * pmod->dfn);
	if (pmod->fstt < 0.0) {
	    pmod->fstt = 0.0;
	}
    }

    diag = malloc(pmod->ncoeff * sizeof *diag); 
    if (diag == NULL) {
	pmod->errcode = E_ALLOC;
	return;
    }

    /* Note: 'xpy' is used purely as workspace in diaginv() below, as
       a matter of economy/convenience; no values in this array are
       used before they are written. 
    */
    diaginv(pmod->xpx, xpy, diag, pmod->ncoeff);

    for (i=0; i<pmod->ncoeff; i++) {
	if (diag[i] >= 0.0) {
	    pmod->sderr[i] = pmod->sigma * sqrt(diag[i]);
	} else {
	    pmod->sderr[i] = 0.0;
	}
    }

    free(diag); 
}

/**
 * makevcv:
 * @pmod: pointer to model.
 * @sigma: square root of error variance, or 1.0 to
 * produce just X'X^{-1}.
 *
 * Inverts the Cholesky-decomposed X'X and computes the 
 * coefficient covariance matrix.
 * 
 * Returns: 0 on successful completion, non-zero code on error.
 */

int makevcv (MODEL *pmod, double sigma)
{
    int dec, mst, kk, i, j, kj, icnt, m, k, l = 0;
    int nv, nxpx;
    double d;

    if (pmod->vcv != NULL) {
	/* already done */
	return 0;
    }

    if (pmod->xpx == NULL) {
	/* raw material not available */
	fprintf(stderr, "makevcv: pmod->xpx = NULL\n");
	return E_BADSTAT;
    }

    nv = pmod->ncoeff;
    nxpx = (nv * nv + nv) / 2; 
    mst = nxpx;
    kk = nxpx - 1;

    pmod->vcv = malloc(nxpx * sizeof *pmod->vcv);
    if (pmod->vcv == NULL) {
	return E_ALLOC;
    }

    for (i=0; i<nv; i++) {
	mst -= i;
	/* find diagonal element */
	d = pmod->xpx[kk];
	if (i > 0) {
	    for (j=kk+1; j<=kk+i; j++) {
		d -= pmod->xpx[j] * pmod->vcv[j];
	    }
	}
	pmod->vcv[kk] = d * pmod->xpx[kk];
	/* find off-diagonal elements indexed by kj */
	kj = kk;
	kk = kk - i - 2;
	if (i > nv - 2) {
	    continue;
	}
	for (j=i+1; j<nv; j++) {
	    icnt = i+1;
	    kj -= j;
	    d = 0.0;
	    m = mst + 1;
	    for (k=0; k<=j-1; k++) {
		if (icnt > 0) {
		    dec = 1;
		    icnt--;
		} else {
		    dec = k;
		}
		m -= dec;
		l = kj + i - k;
		d += pmod->vcv[m-1] * pmod->xpx[l];
	    }
	    pmod->vcv[kj] = -d * pmod->xpx[l-1];
	}
    }

    if (pmod->ci == LOGIT || pmod->ci == PROBIT) {
	sigma = 1.0;
    }

    if (sigma != 1.0) {
	double s2 = sigma * sigma;

	for (i=0; i<nxpx; i++) {
	    pmod->vcv[i] *= s2;
	}
    }

    return 0;
}

/**
 * dwstat:
 * @order: order of autoregression (usually 1).
 * @pmod: pointer to model.
 * @dset: pointer to dataset.
 *
 * Computes the Durbin-Watson statistic for @pmod.
 * 
 * Returns: the D-W value, or #NADBL on error.
 */

double dwstat (int order, MODEL *pmod, const DATASET *dset)
{
    double ut, u1;
    double num = 0.0;
    double den = 0.0;
    int t, t1;

    if (pmod->ess <= 0.0) {
	return NADBL;
    }

    t1 = pmod->t1 + order;

    if (pmod->nwt) {
	ut = pmod->uhat[t1 - 1];
	if (!na(ut)) {
	    den += ut * ut;
	}
    } else {
	den = pmod->ess;
    }

    for (t=t1; t<=pmod->t2; t++)  {
        ut = pmod->uhat[t];
        u1 = pmod->uhat[t-1];
        if (na(ut) || na(u1) ||
	    (pmod->nwt && (dset->Z[pmod->nwt][t] == 0.0 || 
			   dset->Z[pmod->nwt][t-1] == 0.0))) { 
	    continue;
	}
        num += (ut - u1) * (ut - u1);
	if (pmod->nwt) {
	    den += ut * ut;
	}
    }

    return num / den;
}

/* altrho: alternative calculation of rho */

static double altrho (int order, int t1, int t2, const double *uhat)
{
    double *ut, *u1;    
    int t, n, len = t2 - (t1 + order) + 1;
    double uht, uh1, rho;

    ut = malloc(len * sizeof *ut);
    if (ut == NULL) {
	return NADBL;
    }

    u1 = malloc(len * sizeof *u1);
    if (u1 == NULL) {
	free(ut);
	return NADBL;
    }

    n = 0;

    for (t=t1+order; t<=t2; t++) { 
        uht = uhat[t];
	uh1 = (t > 0)? uhat[t-1] : NADBL;
        if (!na(uht) && !na(uh1)) {
	    ut[n] = uht;
	    u1[n] = uh1;
	    n++;
	}
    }

    rho = gretl_corr(0, n - 1, ut, u1, NULL);

    free(ut);
    free(u1);

    return rho;
}

/**
 * rhohat:
 * @order: order of autoregression, usually 1.
 * @t1: start of sample range.
 * @t2: end of sample range.
 * @uhat: array of regression residuals.
 *
 * Computes the first order serial correlation coefficient
 * for @uhat, over the range @t1 to @t2.
 * 
 * Returns: the \hat{rho} value, or #NADBL on error.
 */

double rhohat (int order, int t1, int t2, const double *uhat)
{
    double ut, u1, uu = 0.0, xx = 0.0;
    double rho;
    int t;

    for (t=t1+order; t<=t2; t++) { 
        ut = uhat[t];
        u1 = uhat[t-1];
        if (na(ut) || na(u1)) {
	    continue;
	}
        uu += ut * u1;
        xx += u1 * u1;
    }

    if (xx < DBL_EPSILON) {
	return NADBL;
    }

    rho = uu / xx;

    if (rho > 1.0 || rho < -1.0) {
	rho = altrho(order, t1, t2, uhat);
    }

    return rho;
}

/* compute fitted values and residuals */

static int hatvar (MODEL *pmod, int n, const DATASET *dset)
{
    int yno = pmod->list[1];
    int xno, i, t;
    double x;

    for (t=0; t<n; t++) {
	pmod->yhat[t] = pmod->uhat[t] = NADBL;
    }

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (model_missing(pmod, t)) {
	    continue;
	}
	pmod->yhat[t] = 0.0;
        for (i=0; i<pmod->ncoeff; i++) {
            xno = pmod->list[i+2];
	    x = dset->Z[xno][t];
	    if (pmod->nwt) {
		x *= sqrt(dset->Z[pmod->nwt][t]);
	    }
            pmod->yhat[t] += pmod->coeff[i] * x;
        }
	x = dset->Z[yno][t];
	if (pmod->nwt) {
	    x *= sqrt(dset->Z[pmod->nwt][t]);
	}
        pmod->uhat[t] = x - pmod->yhat[t];
    }

    return 0;
}

static int hilu_plot (double *ssr, double *rho, int n)
{
    FILE *fp;
    int i, err = 0;

    fp = open_plot_input_file(PLOT_REGULAR, &err);
    if (err) {
	return err; 
    }

    fputs("# hildreth-lu\n", fp);
    fputs("set xlabel 'rho'\n", fp);

    fprintf(fp, "set ylabel '%s'\n", _("ESS"));

    fputs("set nokey\n", fp);
    fputs("set xrange [-1.0:1.0]\n", fp);
    fputs("plot '-' using 1:2 w impulses\n", fp);

    gretl_push_c_numeric_locale();

    for (i=0; i<n; i++) {
	fprintf(fp, "%g %g\n", rho[i], ssr[i]);
    }
    fputs("e\n", fp);

    gretl_pop_c_numeric_locale();

    return finalize_plot_input_file(fp);
}

static double autores (MODEL *pmod, const double **Z, gretlopt opt)
{
    int t, v, t1 = pmod->t1;
    double x, num = 0.0, den = 0.0;
    double rhohat;

    if (!(opt & OPT_P)) {
	/* not using Prais-Winsten */
	t1--;
    }

    for (t=t1; t<=pmod->t2; t++) {
	x = Z[pmod->list[1]][t];
	for (v=0; v<pmod->ncoeff; v++) {
	    x -= pmod->coeff[v] * Z[pmod->list[v+2]][t];
	}
	pmod->uhat[t] = x;
	if (t > t1) {
	    num += pmod->uhat[t] * pmod->uhat[t-1];
	    den += pmod->uhat[t-1] * pmod->uhat[t-1];
	}
    } 

    rhohat = num / den;

    return rhohat;
}

#define AR_DEBUG 0

/*
 * estimate_rho:
 * @list: dependent variable plus list of regressors.
 * @dset: dataset struct.
 * @opt: option flags: may include OPT_H to use Hildreth-Lu,
 *       OPT_P to use Prais-Winsten, OPT_B to suppress Cochrane-Orcutt
 *       fine-tuning of Hildreth-Lu results, OPT_G to generate
 *       a gnuplot graph of the search in Hildreth-Lu case.
 * @prn: gretl printing struct.
 * @err: location to receeve error code.
 *
 * Estimate the quasi-differencing coefficient for use with the
 * Cochrane-Orcutt, Hildreth-Lu or Prais-Winsten procedures for
 * handling first-order serial correlation.  Print a trace of the
 * search for rho.
 * 
 * Returns: rho estimate on successful completion, #NADBL on error.
 */

static double estimate_rho (const int *list, DATASET *dset,
			    gretlopt opt, PRN *prn, int *err)
{
    double rho = 0.0, rho0 = 0.0, diff;
    double finalrho = 0.0, essmin = 1.0e8;
    double ess, ssr[199], rh[199]; 
    int iter, nn = 0;
    int t1 = dset->t1, t2 = dset->t2;
    gretlopt lsqopt = OPT_A;
    int quiet = (opt & OPT_Q);
    int ascii = !(opt & OPT_G);
    MODEL armod;

    *err = list_adjust_sample(list, &dset->t1, &dset->t2, dset, NULL);
    if (*err) {
	goto bailout;
    }

    gretl_model_init(&armod, dset);

    if (opt & OPT_P) {
	/* Prais-Winsten treatment of first observation */
	lsqopt |= OPT_P;
    } 

    if (opt & OPT_H) { 
	/* Do Hildreth-Lu first */
	for (rho = -0.990, iter = 0; rho < 1.0; rho += .01, iter++) {
	    clear_model(&armod);
	    armod = ar1_lsq(list, dset, OLS, OPT_A, rho);
	    if ((*err = armod.errcode)) {
		clear_model(&armod);
		goto bailout;
	    }
	    ess = armod.ess;
	    if (ascii && !quiet) {
		char num[16];
		int chk;
		
		if (iter == 0) {
		    pprintf(prn, "\n RHO       %s      RHO       %s      "
			    "RHO       %s      RHO       %s     \n",
			    _("ESS"), _("ESS"), _("ESS"), _("ESS"));
		}
		sprintf(num, "%f", 100 * fabs(rho));
		chk = atoi(num);
		if (chk == 99 || chk % 10 == 0) {
		    ssr[nn] = ess;
		    rh[nn++] = rho;
		    pprintf(prn, "%5.2f %10.4g", rho, ess);
		    if (nn % 4 == 0) {
			pputc(prn, '\n');
		    } else {
			bufspace(3, prn);
		    }
		} 
	    } else if (!quiet) {
		ssr[nn] = ess;
		rh[nn++] = rho;
	    }
	    if (iter == 0 || ess < essmin) {
		essmin = ess;
		finalrho = rho;
	    }
	} /* end of basic iteration */
	
	if (finalrho > 0.989) {
	    /* try exploring this funny region? */
	    for (rho = 0.99; rho <= 0.999; rho += .001) {
		clear_model(&armod);
		armod = ar1_lsq(list, dset, OLS, OPT_A, rho);
		if ((*err = armod.errcode)) {
		    clear_model(&armod);
		    goto bailout;
		}
		ess = armod.ess;
		if (ess < essmin) {
		    essmin = ess;
		    finalrho = rho;
		}
	    }
	}

	if (finalrho > 0.9989) {
	    /* this even funnier one? */
	    for (rho = 0.9991; rho <= 0.9999; rho += .0001) {
		clear_model(&armod);
		armod = ar1_lsq(list, dset, OLS, OPT_A, rho);
		if ((*err = armod.errcode)) {
		    clear_model(&armod);
		    goto bailout;
		}
		ess = armod.ess;
		if (ess < essmin) {
		    essmin = ess;
		    finalrho = rho;
		}
	    }
	}

	rho0 = rho = finalrho;
	if (!quiet) {
	    pprintf(prn, _("\n\nESS is minimum for rho = %g\n\n"), rho);
	    if (ascii) {
		graphyx(ssr, rh, nn, "ESS", "RHO", prn); 
		pputs(prn, "\n");
	    } else {
		hilu_plot(ssr, rh, nn);
	    }
	}
    } else { 
	/* Go straight to Cochrane-Orcutt (or Prais-Winsten) */
	armod = lsq(list, dset, OLS, OPT_A);
	if (!armod.errcode && armod.dfd == 0) {
	    armod.errcode = E_DF;
	}
	if ((*err = armod.errcode)) {
	    clear_model(&armod);
	    goto bailout;
	}
	rho0 = rho = armod.rho;
    }

    if (na(rho)) {
	*err = E_NOCONV;
	clear_model(&armod);
	goto bailout;
    }

    if (!(opt & OPT_H) || !(opt & OPT_B)) {
	if (!quiet) {
	    if (opt & OPT_H) {
		pputs(prn, _("\nFine-tune rho using the CORC procedure...\n\n"));
	    } else {
		pputs(prn, _("\nPerforming iterative calculation of rho...\n\n"));
	    }

	    pputs(prn, _("                 ITER       RHO        ESS"));
	    pputc(prn, '\n');
	}

	iter = 0;
	diff = 1.0;

	while (diff > 0.001) {
	    clear_model(&armod);
	    armod = ar1_lsq(list, dset, OLS, lsqopt, rho);
#if AR_DEBUG
	    fprintf(stderr, "armod: t1=%d, first two uhats: %g, %g\n",
		    armod.t1, 
		    armod.uhat[armod.t1],
		    armod.uhat[armod.t1+1]);
#endif
	    if ((*err = armod.errcode)) {
		clear_model(&armod);
		goto bailout;
	    }
	    if (!quiet) {
		pprintf(prn, "          %10d %12.5f", ++iter, rho);
		pprintf(prn, "   %g\n", armod.ess);
	    }

	    rho = autores(&armod, (const double **) dset->Z, opt);

#if AR_DEBUG
	    pputs(prn, "AR1 model (using rho-transformed data)\n");
	    printmodel(&armod, dset, OPT_NONE, prn);
	    pprintf(prn, "autores gives rho = %g\n", rho);
#endif

	    if (rho > .99999 || rho < -.99999) {
		*err = E_NOCONV;
		clear_model(&armod);
		goto bailout;
	    }

	    diff = (rho > rho0)? rho - rho0 : rho0 - rho;
	    rho0 = rho;
	    if (iter == 30) break;
	}

	if (!quiet) {
	    pprintf(prn, "          %10d %12.5f", ++iter, rho);
	    pprintf(prn, "   %g\n", armod.ess);
	}
    }

    clear_model(&armod);

 bailout:

    dset->t1 = t1;
    dset->t2 = t2;

    if (*err) {
	rho = NADBL;
    }

    return rho;
}

/**
 * augment_regression_list:
 * @orig: list giving original regression specification.
 * @aux: either %AUX_SQ, %AUX_LOG or %AUX_WHITE.
 * @dset: dataset struct.
 * @err: location to receive error code.
 *
 * Augment the regression list @orig with auxiliary terms.  If @aux 
 * is %AUX_SQ add the squares of the original regressors; if @aux
 * is %AUX_WHITE add squares and cross-products, or if @aux is
 * %AUX_LOG add the natural logs of the original regressors.
 * If they are not already present, these variables are added
 * to the data array.
 * 
 * Returns: the augmented list, or NULL on failure.
 */

int *augment_regression_list (const int *orig, int aux, 
			      DATASET *dset, int *err)
{
    int *list;
    int listlen;
    int cnum = 0;
    int i, k;

    if (aux == AUX_WHITE) {
	int cpos = gretl_list_const_pos(orig, 2, dset);
	int nt, trv = orig[0] - 1;

	if (cpos > 0) {
	    trv--;
	    cnum = orig[cpos];
	}
	nt = (trv * trv + trv) / 2;
	listlen = orig[0] + nt + 1;
    } else {
	listlen = 2 * orig[0];
    }

    list = malloc(listlen * sizeof *list);
    if (list == NULL) {
	return NULL;
    }

    /* transcribe original list */
    for (i=0; i<=orig[0]; i++) {
	list[i] = orig[i];
    }

    /* add squares, cross-products or logs of independent vars */

    k = list[0];
    for (i=2; i<=orig[0]; i++) {
	int vnew, vi = orig[i];

	if (vi == 0) {
	    continue;
	}

	if (aux == AUX_SQ || aux == AUX_WHITE) {
	    vnew = xpxgenr(vi, vi, dset);
	    if (vnew > 0) {
		list[++k] = vnew;
	    }
	    if (aux == AUX_WHITE) {
		int j, vj;

		for (j=i+1; j<=orig[0]; j++) {
		    vj = orig[j];
		    if (vj == cnum) {
			continue;
		    }
		    vnew = xpxgenr(vi, vj, dset);
		    if (vnew > 0) {
			/* ensure uniqueness of generated varnames */
			sprintf(dset->varname[vnew], "X%d_X%d", i-1, j-1);
			list[++k] = vnew;
		    }
		}
	    }
	} else if (aux == AUX_LOG) {
	    /* don't try to log series with non-positive values */
	    if (gretl_ispositive(dset->t1, dset->t2, dset->Z[vi], 1)) {
		vnew = loggenr(vi, dset);
		if (vnew > 0) {
		    list[++k] = vnew;
		}
	    }
	}
    }

    list[0] = k;

    return list;
}

/* For observation s, see if the regression list contains a variable
   that has a single non-zero value at that particular observation.
   We run this check on observations showing an OLS residual of zero.
*/

static int observation_is_dummied (const MODEL *pmod,
				   int *list, const double **Z,
				   int s)
{
    int i, t, v;
    int ret = 0;

    for (i=list[0]; i>=2; i--) {
	v = list[i];
	if (v == 0) {
	    continue;
	}
	ret = 1;
	for (t=pmod->t1; t<=pmod->t2; t++) {
	    if ((t == s && Z[v][t] == 0.0) || (t != s && Z[v][t] != 0.0)) {
		ret = 0;
		break;
	    }
	}
	if (ret) {
	    gretl_list_delete_at_pos(list, i);
	    break;
	}
    }

    return ret;
}

/* get_hsk_weights: take the residuals from the model pmod, square them
   and take logs; find the fitted values for this series using an
   auxiliary regression including the original independent variables
   and their squares; exponentiate the fitted values; and add the
   resulting series to the data set.
*/

static int get_hsk_weights (MODEL *pmod, DATASET *dset)
{
    int oldv = dset->v;
    int t, t1 = dset->t1, t2 = dset->t2;
    int *lcpy = NULL;
    int *list = NULL;
    int err = 0, shrink = 0;
    double xx;
    MODEL aux;

    lcpy = gretl_list_copy(pmod->list);
    if (lcpy == NULL) {
	return E_ALLOC;
    }

    /* allocate space for an additional variable */
    if (dataset_add_series(dset, 1)) {
	free(lcpy);
	return E_ALLOC;
    }

    /* add transformed pmod residuals to data set */
    for (t=0; t<dset->n; t++) {
	xx = pmod->uhat[t];
	if (na(xx)) {
	    dset->Z[oldv][t] = NADBL;
	} else if (xx == 0.0) {
	    if (observation_is_dummied(pmod, lcpy, (const double **) dset->Z, t)) {
		dset->Z[oldv][t] = NADBL;
	    } else {
		fprintf(stderr, "hsk: got a zero residual, could be a problem!\n");
		dset->Z[oldv][t] = -1.0e16; /* ?? */
	    }
	} else {
	    dset->Z[oldv][t] = log(xx * xx);
	}
    }

    /* build regression list, adding the squares of the original
       independent vars */
    list = augment_regression_list(lcpy, AUX_SQ, dset, &err);
    if (err) {
	return err;
    }

    list[1] = oldv; /* the newly added log(uhat-squared) */

    dset->t1 = pmod->t1;
    dset->t2 = pmod->t2;

    aux = lsq(list, dset, OLS, OPT_A);
    err = aux.errcode;
    if (err) {
	shrink = dset->v - oldv;
    } else {
	/* write into the data set the required weights */
	for (t=aux.t1; t<=aux.t2; t++) {
	    xx = aux.yhat[t];
	    if (na(xx)) {
		dset->Z[oldv][t] = NADBL;
	    } else {
		dset->Z[oldv][t] = 1.0 / exp(xx);
	    }
	}
	shrink = dset->v - oldv - 1;
    }

    dset->t1 = t1;
    dset->t2 = t2;

    clear_model(&aux);

    if (shrink > 0) {
	dataset_drop_last_variables(dset, shrink);
    }

    free(list);
    free(lcpy);

    return err;
}

/**
 * hsk_model:
 * @list: dependent variable plus list of regressors.
 * @dset: dataset struct.
 *
 * Estimate the model given in @list using a correction for
 * heteroskedasticity.
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL hsk_model (const int *list, DATASET *dset)
{
    int i, err;
    int orig_nvar = dset->v;
    int *hsklist;
    MODEL hsk;

    if (dset->Z == NULL) {
	hsk.errcode = E_DATA;
	return hsk;
    }

    gretl_error_clear();

    /* run initial OLS */
    hsk = lsq(list, dset, OLS, OPT_A);
    if (hsk.errcode) {
	return hsk;
    }

    /* use the residuals from the initial OLS to form weights */
    err = get_hsk_weights(&hsk, dset);
    if (err) {
	hsk.errcode = err;
	return hsk;
    }

    /* allocate regression list for weighted least squares */
    hsklist = gretl_list_new(list[0] + 1);
    if (hsklist == NULL) {
	hsk.errcode = E_ALLOC;
	return hsk;
    }

    /* the last variable in the dataset will be the weight var */
    hsklist[1] = dset->v - 1;

    /* put the original dependent variable in at position 2 */
    hsklist[2] = list[1];

    /* add the original independent vars into the WLS list */
    for (i=3; i<=hsklist[0]; i++) {
	hsklist[i] = list[i-1];
    }

    clear_model(&hsk);
    hsk = lsq(hsklist, dset, WLS, OPT_NONE);
    hsk.ci = HSK;

    dataset_drop_last_variables(dset, dset->v - orig_nvar);

    free(hsklist);

    return hsk;
}

static int jackknife_vcv (MODEL *pmod, const DATASET *dset)
{
    double *st = NULL, *ustar = NULL;
    double **p = NULL;
    int nobs, tp, nc = 0;
    int i, j, k, t;
    int t1, t2;
    double xx;
    int err = 0;

    t1 = pmod->t1;
    t2 = pmod->t2;
    nobs = pmod->nobs;
    nc = pmod->ncoeff;

    st = malloc(nc * sizeof *st);
    ustar = malloc(nobs * sizeof *ustar);
    p = doubles_array_new(nc, nobs);

    if (st == NULL || p == NULL || ustar == NULL) {
	err = E_ALLOC;
	goto bailout;
    }  

    if (pmod->vcv != NULL) {
	free(pmod->vcv);
	pmod->vcv = NULL;
    }

    if (makevcv(pmod, 1.0)) {
	err = E_ALLOC;
	goto bailout;
    }

    /* form elements of (X'X)^{-1}X' */

    for (i=0; i<nc; i++) {
	tp = 0;
	for (t=t1; t<=t2; t++) {
	    if (model_missing(pmod, t)) {
		continue;
	    }
	    xx = 0.0;
	    for (j=0; j<nc; j++) {
		if (i <= j) {
		    k = ijton(i, j, nc);
		} else {
		    k = ijton(j, i, nc);
		}
		xx += pmod->vcv[k] * dset->Z[pmod->list[j+2]][t];
	    }
	    p[i][tp++] = xx;
	}
    }

    tp = 0;
    for (t=t1; t<=t2; t++) {
	if (model_missing(pmod, t)) {
	    continue;
	}	
	xx = 0.0;
	for (i=0; i<nc; i++) {
	    xx += dset->Z[pmod->list[i+2]][t] * p[i][tp];
	}
	if (floateq(xx, 1.0)) {
	    xx = 0.0;
	}
	ustar[tp++] = pmod->uhat[t] / (1.0 - xx);
    }

    for (i=0; i<nc; i++) {
	xx = 0.0;
	for (t=0; t<nobs; t++) {
	    xx += p[i][t] * ustar[t];
	}
	st[i] = xx;
    }

    for (t=0; t<nobs; t++) {
	for (i=0; i<nc; i++) {
	    p[i][t] *= ustar[t];
	}
    }

    /* MacKinnon and White, 1985, equation (13) */

    k = 0;
    for (i=0; i<nc; i++) {
	for (j=i; j<nc; j++) {
	    xx = 0.0;
	    for (t=0; t<nobs; t++) {
		xx += p[i][t] * p[j][t];
	    }
	    xx -= st[i] * st[j] / nobs;
	    /* MacKinnon and White: "It is tempting to omit the factor
	       (n - 1) / n from HC3" (1985, p. 309).  Here we leave it in
	       place, as in their simulations.
	    */
	    xx *= (nobs - 1.0) / nobs;
	    if (i == j) {
		pmod->sderr[i] = sqrt(xx);
	    }
	    pmod->vcv[k++] = xx;
	}
    }

    /* substitute robust F stat */
    if (pmod->dfd > 0 && pmod->dfn > 1) {
	pmod->fstt = wald_omit_F(NULL, pmod);
    }

    pmod->opt |= (OPT_R | OPT_J);
    gretl_model_set_vcv_info(pmod, VCV_HC, 4);

 bailout:

    free(st);
    free(ustar);
    doubles_array_free(p, nc);

    return err;
}

static void print_HET_1 (double z, double pval, PRN *prn)
{
    pprintf(prn, "\n%s\n", _("Pesaran-Taylor test for heteroskedasticity"));
    pprintf(prn, "\n%s: HET_1 = %f,\n", _("Test statistic"), z);
    pprintf(prn, "%s = 2 * P(z > %f) = %.3g\n\n", 
	    _("with p-value"), z, pval);
}

static void print_whites_test (double LM, int df, double pval, 
			       gretlopt opt, PRN *prn)
{
    if (opt & OPT_B) {
	pprintf(prn, "\n%s\n", _("Breusch-Pagan test for heteroskedasticity"));
	if (opt & OPT_R) {
	    pprintf(prn, "%s\n", _("with Koenker robust variance estimator"));
	}
	pprintf(prn, "\n%s: LM = %f,\n", _("Test statistic"), LM);
    } else {
	pprintf(prn, "\n%s", _("White's test for heteroskedasticity"));
	if (opt & OPT_X) {
	    pprintf(prn, " (%s)\n", _("squares only"));
	} else {
	    pputc(prn, '\n');
	}
	pprintf(prn, "\n%s: TR^2 = %f,\n", _("Test statistic"), LM);
    }

    pprintf(prn, "%s = P(%s(%d) > %f) = %f\n\n", 
	    _("with p-value"), _("Chi-square"), 
	    df, LM, pval); 
}

/* For White's test, see if we have enough degrees of freedom
   to add squares -- and if so, if we have enough df to add
   cross-products also.
*/

static int get_whites_aux (const MODEL *pmod, const double **Z) 
{
    int aux = AUX_WHITE;
    int rem = pmod->ncoeff - pmod->ifc - 1;
    int nsq = 0, nxpx = 0;
    int i, v;

    for (i=2; i<=pmod->list[0]; i++) {
	v = pmod->list[i];
	if (v > 0) {
	    if (!gretl_isdummy(pmod->t1, pmod->t2, Z[v])) {
		nsq++;
	    }
	    nxpx += rem--;
	}
    }

    if (pmod->dfd - nsq < 1) {
	aux = AUX_NONE;
    } else if (pmod->dfd - nsq - nxpx < 1) {
	aux = AUX_SQ;
    }

    return aux;
}

#define PT_DEBUG 0

/**
 * tsls_hetero_test:
 * @pmod: pointer to model.
 * @dset: dataset struct.
 * @opt: if flags include OPT_S, save results to model; OPT_Q
 * means don't print the auxiliary regression.
 * @prn: gretl printing struct.
 *
 * Runs Pesaran and Taylor's (1999) HET_1 test for heteroskedasticity
 * on the given tsls model. The statistic is just a t-statistic, so
 * under the null it is distributed as a standard normal.
 * 
 * Returns: 0 on successful completion, error code on error.
 */

static int tsls_hetero_test (MODEL *pmod, DATASET *dset, 
			     gretlopt opt, PRN *prn)
{
    int pos, newv = dset->v;
    int *ptlist = NULL, *testlist = NULL;
    int save_t1 = dset->t1;
    int save_t2 = dset->t2;
    MODEL ptmod;
    double x;
    int i, h, t;
    int err = 0;

    pos = gretl_list_separator_position(pmod->list);
    h = pmod->list[0] - pos;

#if PT_DEBUG
    pprintf(prn, "v = %d, h = %d\n", v, h);
#endif

    ptlist = gretl_list_new(h + 1);
    testlist = gretl_list_new(3);

    if (ptlist == NULL || testlist == NULL) {
	free(ptlist);
	free(testlist);
	return E_ALLOC;
    }

    ptlist[1] = pmod->list[1];
    for (i=2; i<=ptlist[0]; i++) {
	ptlist[i] = pmod->list[i + pos - 1];
    }	

    testlist[1] = newv;
    testlist[2] = 0;
    testlist[3] = newv + 1;

#if PT_DEBUG
    printlist(ptlist, "ptlist");
    printlist(testlist, "testlist");
#endif

    /* reduced form: regress the original dependent variable on all of
       the instruments from the original model
    */
    ptmod = lsq(ptlist, dset, OLS, OPT_A);
    err = ptmod.errcode;
    if (err) {
	goto bailout;
    }

#if PT_DEBUG
    printmodel(&ptmod, dset, OPT_S, prn);
#endif

    /* add two series: (i) the squares of the residuals from the
       original model and (ii) the squares of the fitted values from
       the auxiliary regression above
     */
    err = dataset_add_series(dset, 2);
    if (err) {
	clear_model(&ptmod);
	goto bailout;
    }

    strcpy(dset->varname[newv+1], "yhat^2");

    for (t=pmod->t1; t<=pmod->t2; t++) {
	x = pmod->uhat[t];
	dset->Z[newv][t] = x*x;   /* squared residual */
	x = ptmod.yhat[t];
	dset->Z[newv+1][t] = x*x; /* squared fitted value */
    }

    clear_model(&ptmod);

    dset->t1 = pmod->t1;
    dset->t2 = pmod->t2;

    /* regress the squared residuals on the squared fitted
       values from the reduced-form auxiliary regression
    */
    ptmod = lsq(testlist, dset, OLS, OPT_A);
    err = ptmod.errcode;

    if (!err) {
	double z = fabs(ptmod.coeff[1]) / ptmod.sderr[1];
	double pval = 2.0 * (1 - normal_cdf(z));

	if (opt & OPT_Q) {
	    print_HET_1(z, pval, prn);
	} else {
	    ptmod.aux = AUX_HET_1;
	    printmodel(&ptmod, dset, OPT_NONE, prn);
	}

	if (opt & OPT_S) {
	    ModelTest *test = model_test_new(GRETL_TEST_HET_1);

	    if (test != NULL) {
		model_test_set_teststat(test, GRETL_STAT_Z);
		model_test_set_value(test, z);
		model_test_set_pvalue(test, pval);
		maybe_add_test_to_model(pmod, test);
	    }	  
	}

	record_test_result(z, pval, _("HET_1"));
    }

    clear_model(&ptmod);

    dataset_drop_last_variables(dset, 2); 

 bailout:

    free(ptlist);
    free(testlist);

    dset->t1 = save_t1;
    dset->t2 = save_t2;

    return err;
}

/* Compute LM statistic as per Breusch and Pagan (Econometrica, 1979),
   with the option to use the robust variance estimator proposed by
   Koenker (Journal of Econometrics, 1981).
*/

static double get_BP_LM (MODEL *pmod, int *list, MODEL *aux,
			 DATASET *dset, gretlopt opt, int *err)
{
    double s2, u2t, gt;
    double V = 0.0, LM = NADBL;
    int t, v = list[1];

    s2 = pmod->ess / pmod->nobs;

    if (opt & OPT_R) {
	/* calculate robust variance estimate a la Koenker */
	for (t=pmod->t1; t<=pmod->t2; t++) {
	    if (!na(pmod->uhat[t])) {
		u2t = pmod->uhat[t] * pmod->uhat[t];
		V += (u2t - s2) * (u2t - s2);
	    }
	}
	V /= pmod->nobs;
    }

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (na(pmod->uhat[t])) {
	    dset->Z[v][t] = NADBL;
	} else {
	    u2t = pmod->uhat[t] * pmod->uhat[t];
	    gt = (opt & OPT_R)? (u2t - s2) : (u2t / s2);
	    dset->Z[v][t] = gt;
	}
    }

    *aux = lsq(list, dset, OLS, OPT_A);
    *err = aux->errcode;

    if (!*err) {
	double RSS = aux->tss - aux->ess;

	if (RSS < 0) {
	    *err = E_DATA;
	} else {
	    if (opt & OPT_R) {
		aux->opt |= OPT_R;
		LM = RSS / V;
	    } else {
		LM = .5 * RSS;
	    }
	    gretl_model_set_double(aux, "BPLM", LM);
	    aux->aux = AUX_BP;
	}
    }

    return LM;
}

static int *ensure_const_copy_list (const MODEL *pmod, int *err)
{
    int *list = NULL;

    if (pmod->ifc) {
	list = gretl_list_copy(pmod->list);
    } else {
	int i, nl = pmod->list[0] + 1;
	
	list = gretl_list_new(nl);
	if (list != NULL) {
	    list[1] = pmod->list[1];
	    list[2] = 0; /* insert const */
	    for (i=3; i<=nl; i++) {
		list[i] = pmod->list[i-1];
	    }
	}
    }

    if (list == NULL) {
	*err = E_ALLOC;
    }

    return list;
}

static int *ensure_const_augment_list (const MODEL *pmod, int aux,
				       DATASET *dset, int *err)
{
    int *list = NULL;
    
    if (pmod->ifc) {
	list = augment_regression_list(pmod->list, aux, dset, err);
    } else {
	int *tmp = ensure_const_copy_list(pmod, err);

	if (!*err) {
	    list = augment_regression_list(tmp, aux, dset, err);
	    free(tmp);
	}
    }

    return list;
}

/**
 * whites_test:
 * @pmod: pointer to model.
 * @dset: dataset struct.
 * @opt: if flags include OPT_S, save results to model; OPT_Q
 * means don't print the auxiliary regression;  OPT_B means
 * do the simpler Breusch-Pagan variant.
 * @prn: gretl printing struct.
 *
 * Runs White's test for heteroskedasticity on the given model.
 * 
 * Returns: 0 on successful completion, error code on error.
 */

int whites_test (MODEL *pmod, DATASET *dset, 
		 gretlopt opt, PRN *prn)
{
    int BP = (opt & OPT_B);
    int aux = AUX_NONE;
    int v = dset->v;
    int *list = NULL;
    int save_t1 = dset->t1;
    int save_t2 = dset->t2;
    double zz, LM;
    MODEL white;
    int t, err = 0;

    if (pmod->ci == IVREG) {
	return tsls_hetero_test(pmod, dset, opt, prn);
    }

    if (pmod->list == NULL || gretl_list_has_separator(pmod->list)) {
	return E_NOTIMP;
    }

    if (pmod->ci == NLS || pmod->ci == MLE ||
	pmod->ci == GMM || pmod->ci == ARMA || 
	pmod->ci == LOGISTIC) { 
	return E_NOTIMP;
    }

    if ((err = list_members_replaced(pmod->list, dset, pmod->ID))) {
	return err;
    }

    impose_model_smpl(pmod, dset);

    /* what can we do, with the degrees of freedom available? */
    if (!BP) {
	if (opt & OPT_X) { 
	    aux = AUX_SQ;
	} else {
	    aux = get_whites_aux(pmod, (const double **) dset->Z);
	    if (aux == AUX_NONE) {
		dset->t1 = save_t1;
		dset->t2 = save_t2;
		return E_DF;
	    }
	}
    }

    gretl_model_init(&white, dset);

    /* make space in data set */
    if (dataset_add_series(dset, 1)) {
	err = E_ALLOC;
    }

    if (!err) {
	/* get residuals, square and add to data set */
	for (t=0; t<dset->n; t++) {
	    zz = pmod->uhat[t];
	    if (na(zz)) {
		dset->Z[v][t] = NADBL;
	    } else {
		dset->Z[v][t] = zz * zz;
	    }
	}
	strcpy(dset->varname[v], "uhatsq");
    }

    if (!err) {
	if (BP) {
	    list = ensure_const_copy_list(pmod, &err);
	} else {
	    /* build aux regression list, adding squares and
	       (possibly) cross-products of the original 
	       independent vars */
	    list = ensure_const_augment_list(pmod, aux, dset, &err);
	}
	if (!err){
	    list[1] = v; /* the newly added uhat-squared */
	}
    }

    if (!err) {
	/* run auxiliary regression */
	if (BP) {
	    LM = get_BP_LM(pmod, list, &white, dset, opt, &err);
	} else {
	    white = lsq(list, dset, OLS, OPT_A);
	    err = white.errcode;
	    if (!err) {
		LM = white.rsq * white.nobs;
		white.aux = AUX_WHITE;
	    }
	}
    }

    if (!err) {
	int df = white.ncoeff - 1;
	double pval = chisq_cdf_comp(df, LM);
	gretlopt testopt = OPT_NONE;

	if (BP) {
	    if (opt & OPT_R) {
		/* Koenker robust variant */
		testopt = OPT_R;
	    }
	} else if (opt & OPT_X) {
	    /* squares only */
	    testopt = OPT_X;
	}

	if (opt & OPT_Q) {
	    print_whites_test(LM, df, pval, opt, prn);
	} else {
	    white.opt |= testopt;
	    printmodel(&white, dset, OPT_NONE, prn);
	}

	if (opt & OPT_S) {
	    ModelTestType mt = BP? GRETL_TEST_BP : GRETL_TEST_WHITES;
	    ModelTest *test = model_test_new(mt);
	    
	    if (test != NULL) {
		model_test_set_teststat(test, GRETL_STAT_LM);
		model_test_set_dfn(test, df);
		model_test_set_value(test, LM);
		model_test_set_pvalue(test, pval);
		model_test_set_opt(test, testopt);
		maybe_add_test_to_model(pmod, test);
	    }	  
	}

	if (BP) {
	    record_test_result(LM, pval, "Breusch-Pagan");
	} else {
	    record_test_result(LM, pval, _("White's"));
	}
    }

    clear_model(&white);
    dataset_drop_last_variables(dset, dset->v - v);
    free(list);

    dset->t1 = save_t1;
    dset->t2 = save_t2;

    return err;
}

static int ar_list_max (const int *list) 
{
    int i, lmax = 0;

    for (i=1; i<=list[0]; i++) {
	if (list[i] > lmax) {
	    lmax = list[i];
	}
    }

    return lmax;
}

/**
 * ar_model:
 * @list: list of lags plus dependent variable and list of regressors.
 * @dset: dataset struct.
 * @opt: may contain OPT_O to print covariance matrix.
 * @prn: gretl printing struct.
 *
 * Estimate the model given in @list using the generalized 
 * Cochrane-Orcutt procedure for autoregressive errors.
 * 
 * Returns: #MODEL struct containing the results.
 */

MODEL ar_model (const int *list, DATASET *dset, 
		gretlopt opt, PRN *prn)
{
    double diff, ess, tss, xx;
    int i, j, t, t1, t2, vc, yno, ryno, iter;
    int k, maxlag, v = dset->v;
    int *arlist = NULL, *rholist = NULL;
    int *reglist = NULL, *reglist2 = NULL;
    int cpos, nvadd;
    MODEL ar, rhomod;

    gretl_error_clear();
    gretl_model_init(&ar, dset);
    gretl_model_init(&rhomod, dset);

    ar.errcode = gretl_list_split_on_separator(list, &arlist, &reglist);

    if (!ar.errcode && arlist[0] == 1 && arlist[1] == 1) {
	/* special case: ar 1 ; ... => use AR1 apparatus */
	ar = ar1_model(reglist, dset, opt, prn);
	goto bailout;
    }

    if (!ar.errcode) {
	reglist2 = gretl_list_new(reglist[0]);
	rholist = gretl_list_new(arlist[0] + 1);
	if (reglist2 == NULL || rholist == NULL) {
	    ar.errcode = E_ALLOC;
	}
    }
    
    if (ar.errcode) {
	goto bailout;
    }

    maxlag = ar_list_max(arlist);
    cpos = reglist_check_for_const(reglist, dset);

    /* first pass: estimate model via OLS: use OPT_M to generate an
       error in case of missing values within sample range 
    */
    ar = lsq(reglist, dset, OLS, OPT_A | OPT_M);
    if (ar.errcode) {
	goto bailout;
    }

    nvadd = arlist[0] + 1 + reglist[0];

    /* allocate space for the uhat terms and transformed data */
    if (dataset_add_series(dset, nvadd)) {
	ar.errcode = E_ALLOC;
	goto bailout;
    }

    yno = reglist[1];
    t1 = ar.t1; 
    t2 = ar.t2;
    rholist[1] = v;

    pprintf(prn, "%s\n\n", _("Generalized Cochrane-Orcutt estimation"));
    bufspace(17, prn);
    /* xgettext:no-c-format */
    pputs(prn, _("ITER             ESS           % CHANGE"));
    pputs(prn, "\n\n");

    /* now loop while ess is changing */
    diff = 1.0e6;
    ess = 0.0;

    for (iter = 1; iter <= 20 && diff > 0.005; iter++) {
	for (t=0; t<dset->n; t++) {
	    if (t < t1 || t > t2) {
		dset->Z[v][t] = NADBL;
	    } else {
		/* special computation of uhat */
		xx = dset->Z[yno][t];
		for (j=0; j<reglist[0]-1; j++) {
		    xx -= ar.coeff[j] * dset->Z[reglist[j+2]][t];
		}
		dset->Z[v][t] = xx;
	    }
	}		
	for (i=1; i<=arlist[0]; i++) {
	    k = arlist[i];
	    rholist[1+i] = v + i;
	    for (t=0; t<dset->n; t++) {
		if (t < t1 + k || t > t2) {
		    dset->Z[v+i][t] = NADBL;
		} else {
		    dset->Z[v+i][t] = dset->Z[v][t-k];
		}
	    }
	}

	/* estimate the rho terms */
	if (iter > 1) {
	    clear_model(&rhomod);
	}
	rhomod = lsq(rholist, dset, OLS, OPT_A);

	/* and rho-transform the data */
	ryno = vc = v + i;
	for (i=1; i<=reglist[0]; i++) {
	    for (t=0; t<dset->n; t++) {
		if (t < t1 + maxlag || t > t2) {
		    dset->Z[vc][t] = NADBL;
		} else {
		    xx = dset->Z[reglist[i]][t];
		    for (j=1; j<=arlist[0]; j++) {
			k = arlist[j];
			xx -= rhomod.coeff[j-1] * dset->Z[reglist[i]][t-k];
		    }
		    dset->Z[vc][t] = xx;
		}
	    }
	    reglist2[i] = vc++;
	}

	/* estimate the transformed model */
	clear_model(&ar);
	ar = lsq(reglist2, dset, OLS, OPT_A);

        if (iter > 1) {
	    diff = 100 * (ar.ess - ess) / ess;
	}

        if (diff < 0.0) {
	    diff = -diff;
	}

	ess = ar.ess;
	pprintf(prn, "%16c%3d %20f ", ' ', iter, ess);

	if (iter > 1) {
	    pprintf(prn, "%13.3f\n", diff);
	} else {
	    pputc(prn, '\n');
	}
    } /* end "ess changing" loop */

    for (i=0; i<=reglist[0]; i++) {
	ar.list[i] = reglist[i];
    }
    if (cpos > 0) {
	ar.ifc = 1;
    }
    if (ar.ifc) {
	if (!gretl_model_get_int(&ar, "effconst")) {
	    ar.dfn -= 1;
	}
    }
    ar.ci = AR;

    /* special computation of fitted values */
    for (t=t1; t<=t2; t++) {
	xx = 0.0;
	for (j=2; j<=reglist[0]; j++) { 
	    xx += ar.coeff[j-2] * dset->Z[reglist[j]][t];
	}
	ar.uhat[t] = dset->Z[yno][t] - xx;
	for (j=1; j<=arlist[0]; j++) {
	    if (t - t1 >= arlist[j]) {
		k = arlist[j];
		xx += rhomod.coeff[j-1] * ar.uhat[t-k];
	    }
	}
	ar.yhat[t] = xx;
    }

    for (t=t1; t<=t2; t++) { 
	ar.uhat[t] = dset->Z[yno][t] - ar.yhat[t];
    }

    ar.rsq = gretl_corr_rsq(ar.t1, ar.t2, dset->Z[reglist[1]], ar.yhat);
    ar.adjrsq = 1.0 - ((1.0 - ar.rsq) * (ar.nobs - 1.0) / ar.dfd);

    /* special computation of TSS */
    xx = gretl_mean(ar.t1, ar.t2, dset->Z[ryno]);
    tss = 0.0;
    for (t=ar.t1; t<=ar.t2; t++) {
	tss += (dset->Z[ryno][t] - xx) * (dset->Z[ryno][t] - xx);
    }
    ar.fstt = ar.dfd * (tss - ar.ess) / (ar.dfn * ar.ess);
    ar.lnL = NADBL;
    mle_criteria(&ar, 0);
    ar.dw = dwstat(maxlag, &ar, dset);
    ar.rho = rhohat(maxlag, ar.t1, ar.t2, ar.uhat);

    dataset_drop_last_variables(dset, nvadd);

    if (gretl_model_add_arinfo(&ar, maxlag)) {
	ar.errcode = E_ALLOC;
    } else {
	for (i=0; i<=arlist[0]; i++) { 
	    ar.arinfo->arlist[i] = arlist[i];
	    if (i >= 1) {
		ar.arinfo->rho[i-1] = rhomod.coeff[i-1];
		ar.arinfo->sderr[i-1] = rhomod.sderr[i-1];
	    }
	}
    }
    clear_model(&rhomod);

    set_model_id(&ar);

 bailout:

    free(reglist);
    free(reglist2);
    free(rholist);
    free(arlist);

    return ar;
}

static int modelvar_iszero (const MODEL *pmod, const double *x)
{
    int t;

    for (t=pmod->t1; t<=pmod->t2; t++) {
	if (!model_missing(pmod, t) && floatneq(x[t], 0.0)) {
	    return 0;
	}
    }

    return 1;
}

/* From the position of the first regressorto end of list, omits
   variables with all zero observations and re-packs the rest of
   them */

static void omitzero (MODEL *pmod, const DATASET *dset,
		      gretlopt opt)
{
    int *zlist = NULL;
    int i, v, offset;
    double x = 0.0;

    offset = (pmod->ci == WLS)? 3 : 2;

    if (!(opt & OPT_A)) {
	zlist = gretl_null_list();
    }

    for (i=pmod->list[0]; i>=offset; i--) {
        v = pmod->list[i];
        if (modelvar_iszero(pmod, dset->Z[v])) {
	    if (zlist != NULL) {
		gretl_list_append_term(&zlist, v);
	    }
	    fprintf(stderr, "Deleting var %d (%s) at list pos %d: all zero\n", 
		    v, dset->varname[v], i);
	    gretl_list_delete_at_pos(pmod->list, i);
	}
    }

    if (pmod->nwt) {
	int t, wtzero;

	for (i=pmod->list[0]; i>=offset; i--) {
	    v = pmod->list[i];
	    wtzero = 1;
	    for (t=pmod->t1; t<=pmod->t2; t++) {
		x = dset->Z[v][t] * dset->Z[pmod->nwt][t];
		if (!model_missing(pmod, t) && floatneq(x, 0.0)) {
		    wtzero = 0;
		    break;
		}
	    }
	    if (wtzero) {
		if (zlist != NULL) {
		    gretl_list_append_term(&zlist, v);
		}
		gretl_list_delete_at_pos(pmod->list, i);
	    }
	}
    }

    if (zlist != NULL) {
	if (zlist[0] > 0) {
	    gretl_model_set_list_as_data(pmod, "zerolist", zlist);
	} else {
	    free(zlist);
	}
    }
}

/* lagdepvar: attempt to detect presence of a lagged dependent
   variable among the regressors -- if found, return the position of
   this lagged var in the list; otherwise return 0
*/

static int lagdepvar (const int *list, const DATASET *dset) 
{
    char depvar[VNAMELEN], othervar[VNAMELEN];
    char *p;
    int i, t, ret = 0;

    strcpy(depvar, dset->varname[list[1]]);

    for (i=2; i<=list[0]; i++) {
	if (list[i] == LISTSEP) {
	    break;
	}
	strcpy(othervar, dset->varname[list[i]]);
	p = strrchr(othervar, '_');
	if (p != NULL && isdigit(*(p + 1))) {
	    /* looks like a lag */
	    size_t len = strlen(othervar) - strlen(p);

	    if (!strncmp(depvar, othervar, len)) {
		int gotlag = 1;

		/* strong candidate for lagged depvar, but make sure */
		for (t=dset->t1+1; t<=dset->t2; t++) {
		    if (dset->Z[list[1]][t-1] != dset->Z[list[i]][t]) {
			gotlag = 0;
			break;
		    }
		}
		if (gotlag) {
		    ret = i;
		    break;
		}
	    }
	}
    } 

    return ret;
}

static void arch_test_print_simple (int order, double LM,
				    double pval, gretlopt opt,
				    PRN *prn)
{
    if (opt & OPT_M) {
	/* multi-equation system */
	pprintf(prn, "%s: TR^2 = %f,\n", _("Test statistic"), LM);
    } else {
	pprintf(prn, "\n%s %d\n", _("Test for ARCH of order"), order);
	pprintf(prn, "\n%s: TR^2 = %f,\n", _("Test statistic"), LM);
    }
    pprintf(prn, "%s = P(%s(%d) > %f) = %f\n\n", 
	    _("with p-value"), _("Chi-square"), 
	    order, LM, pval); 
}

static void print_arch_regression (const gretl_matrix *b,
				   const gretl_matrix *V,
				   int T, int order, 
				   gretlopt opt, PRN *prn)
{
    int i, k = order + 1;
    double *se = malloc(k * sizeof *se);
    char **names;

    names = strings_array_new_with_length(k, 16);

    if (se != NULL && names != NULL) {
	if (!(opt & OPT_M)) {
	    /* not multi-equation */
	    pputc(prn, '\n');
	    pprintf(prn, _("Test for ARCH of order %d"), order);
	    pputs(prn, "\n\n");
	}

	for (i=0; i<k; i++) {
	    se[i] = sqrt(gretl_matrix_get(V, i, i));
	    sprintf(names[i], "alpha(%d)", i);
	}

	print_coeffs(b->val, se, (const char **) names, 
		     k, T - k, ARCH, prn);
    }

    free(se);
    strings_array_free(names, k);
}

static void 
arch_test_save_or_print (const gretl_matrix *b, const gretl_matrix *V,
			 int T, int order, double rsq, MODEL *pmod, 
			 gretlopt opt, PRN *prn)
{
    double LM = T * rsq;
    double pv = chisq_cdf_comp(order, LM);

    record_test_result(LM, pv, "ARCH");

    if (V != NULL) {
	/* V will be NULL if --quiet is in force */
	print_arch_regression(b, V, T, order, opt, prn);
    }

    if (opt & OPT_Q) {
	arch_test_print_simple(order, LM, pv, opt, prn);
    }

    if ((opt & OPT_S) || !(opt & OPT_Q)) {
	ModelTest *test = model_test_new(GRETL_TEST_ARCH);

	if (test != NULL) {
	    int heading = (V == NULL);

	    model_test_set_teststat(test, GRETL_STAT_LM);
	    model_test_set_order(test, order);
	    model_test_set_dfn(test, order);
	    model_test_set_value(test, LM);
	    model_test_set_pvalue(test, pv);

	    if (!(opt & OPT_Q)) {
		gretl_model_test_print_direct(test, heading, prn);
	    }

	    if (pmod != NULL && (opt & OPT_S)) {
		maybe_add_test_to_model(pmod, test);
	    } else {
		model_test_free(test);
	    }
	}
    }
}

static int real_arch_test (const double *u, int T, int order, 
			   MODEL *pmod, gretlopt opt, 
			   PRN *prn)
{
    gretl_matrix *X = NULL;
    gretl_matrix *y = NULL;
    gretl_matrix *b = NULL;
    gretl_matrix *V = NULL;
    int i, k, s, t;
    double x, s2, rsq;
    double *ps2 = NULL;
    int err = 0;

    gretl_error_clear();

    if (order < 1 || order > T - 1) {
	gretl_errmsg_sprintf(_("Invalid lag order for arch (%d)"), order);
	return E_DATA;
    }

    T -= order;
    k = order + 1;

    X = gretl_matrix_alloc(T, k);
    y = gretl_column_vector_alloc(T);
    b = gretl_column_vector_alloc(k);

    if (X == NULL || y == NULL || b == NULL) {
	gretl_matrix_free(X);
	gretl_matrix_free(y);
	gretl_matrix_free(b);
	return E_ALLOC;
    }

    if (!(opt & OPT_Q)) {
	V = gretl_matrix_alloc(k, k);
	if (V == NULL) {
	    err = E_ALLOC;
	    goto bailout;
	}
	ps2 = &s2;
    }

    /* fill out the matrices with squared residuals
       and lags of same */

    for (i=0; i<k; i++) {
	for (t=0; t<T; t++) {
	    s = t + order;
	    if (i == 0) {
		x = u[s];
		gretl_vector_set(y, t, x * x);
		gretl_matrix_set(X, t, i, 1.0);
	    } else {
		x = u[s - i];
		gretl_matrix_set(X, t, i, x * x);
	    }
	}
    }

    err = gretl_matrix_ols(y, X, b, V, NULL, ps2);

    if (!err) {
	rsq = gretl_matrix_r_squared(y, X, b, &err);
    }

    if (!err) {
	arch_test_save_or_print(b, V, T, order, rsq, pmod, 
				opt, prn);
    }

 bailout:

    gretl_matrix_free(X);
    gretl_matrix_free(y);
    gretl_matrix_free(b);
    gretl_matrix_free(V);

    return err;
}

/**
 * arch_test:
 * @pmod: model to be tested.
 * @order: lag order for ARCH process.
 * @dset: dataset struct.
 * @opt: if flags include OPT_S, save test results to model;
 * if OPT_Q, be less verbose.
 * @prn: gretl printing struct.
 *
 * Tests @pmod for AutoRegressive Conditional Heteroskedasticity.  
 * 
 * Returns: 0 on success, non-zero code on error.
 */

int arch_test (MODEL *pmod, int order, const DATASET *dset,
	       gretlopt opt, PRN *prn)
{
    int err;

    if (pmod->missmask != NULL) {
	err = E_MISSDATA;
    } else {
	const double *u = pmod->uhat + pmod->t1;

	if (order == 0) {
	    /* use data frequency as default lag order */
	    order = dset->pd;
	}

	err = real_arch_test(u, pmod->nobs, order, pmod, 
			     opt, prn);
    }

    return err;
}

int array_arch_test (const double *u, int n, int order, 
		     gretlopt opt, PRN *prn)
{
    return real_arch_test(u, n, order, NULL, opt, prn);
}

/**
 * arch_model:
 * @list: dependent variable plus list of regressors.
 * @order: lag order for ARCH process.
 * @dset: dataset struct.
 * @opt: may contain OPT_O to print covariance matrix.
 *
 * Estimate the model given in @list via weighted least squares,
 * with the weights based on the predicted error variances from
 * an auxiliary regression of the squared residuals on their lagged
 * values.
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL arch_model (const int *list, int order, DATASET *dset, 
		  gretlopt opt)
{
    MODEL amod;
    int *wlist = NULL, *alist = NULL;
    int T = sample_size(dset);
    int oldv = dset->v;
    int i, t, nwt, k, n = dset->n;
    double *a = NULL;
    double *se = NULL;
    double xx;

    gretl_error_clear();
    gretl_model_init(&amod, dset);

    if (order == 0) {
	/* use data frequency as default lag order */
	order = dset->pd;
    }

    if (order < 1 || order > T - list[0]) {
	amod.errcode = E_UNSPEC;
	gretl_errmsg_sprintf(_("Invalid lag order for arch (%d)"), order);
	return amod;
    }

    if (dataset_add_series(dset, order + 1)) {
	amod.errcode = E_ALLOC;
    } else {
	alist = gretl_list_new(order + 2);
	if (alist == NULL) {
	    amod.errcode = E_ALLOC;
	}
    }

    if (amod.errcode) {
	goto bailout;
    }

    /* start list for aux regression */
    alist[1] = dset->v - order - 1;
    alist[2] = 0;

    /* run initial OLS and get squared residuals */
    amod = lsq(list, dset, OLS, OPT_A | OPT_M);
    if (amod.errcode) {
	goto bailout;
    }

    k = dset->v - order - 1;
    strcpy(dset->varname[k], "utsq");
    for (t=0; t<n; t++) {
	dset->Z[k][t] = NADBL;
    }
    for (t=amod.t1; t<=amod.t2; t++) {
	xx = amod.uhat[t];
	dset->Z[k][t] = xx * xx;
    }
    /* also lags of squared resids */
    for (i=1; i<=order; i++) {
	k =  dset->v - order + i - 1;
	alist[i+2] = k;
	sprintf(dset->varname[k], "utsq_%d", i);
	for (t=0; t<n; t++) {
	    dset->Z[k][t] = NADBL;
	}
	for (t=amod.t1+i; t<=amod.t2; t++) {
	    dset->Z[k][t] = dset->Z[alist[1]][t-i];
	}
    }

    /* run auxiliary regression */
    clear_model(&amod);
    amod = lsq(alist, dset, OLS, OPT_A);
    if (amod.errcode) {
	goto bailout;
    }

    /* steal the coefficients for reference */
    a = amod.coeff;
    amod.coeff = NULL;
    se = amod.sderr;
    amod.sderr = NULL;

    /* do weighted estimation */
    wlist = gretl_list_new(list[0] + 1);

    if (wlist == NULL) {
	amod.errcode = E_ALLOC;
    } else {
	/* construct the weight variable */
	nwt = wlist[1] = dset->v - 1;
	strcpy(dset->varname[nwt], "1/sigma");

	for (i=2; i<=wlist[0]; i++) {
	    wlist[i] = list[i-1];
	}

	k = dset->v - order - 1;

	for (t=amod.t1; t<=amod.t2; t++) {
	    xx = amod.yhat[t];
	    if (xx <= 0.0) {
		xx = dset->Z[k][t];
	    }
	    dset->Z[nwt][t] = 1.0 / xx; /* FIXME is this right? */
	}

	clear_model(&amod);
	amod = lsq(wlist, dset, WLS, OPT_NONE);
	amod.ci = ARCH;

	if (!amod.errcode) {
	    gretl_model_set_int(&amod, "arch_order", order);
	    gretl_model_set_data(&amod, "arch_coeff", a,
				 GRETL_TYPE_DOUBLE_ARRAY,
				 (order + 1) * sizeof *a);
	    gretl_model_set_data(&amod, "arch_sderr", se,
				 GRETL_TYPE_DOUBLE_ARRAY,
				 (order + 1) * sizeof *se);
	}
    }

 bailout:

    if (alist != NULL) free(alist);
    if (wlist != NULL) free(wlist);

    dataset_drop_last_variables(dset, dset->v - oldv); 

    return amod;
}

/**
 * lad:
 * @list: dependent variable plus list of regressors.
 * @dset: dataset struct.
 *
 * Estimate the model given in @list using the method of Least
 * Absolute Deviation (LAD).
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL lad (const int *list, DATASET *dset)
{
    MODEL mod;
    void *handle;
    int (*lad_driver) (MODEL *, DATASET *);

    /* run an initial OLS to "set the model up" and check for errors.
       the lad_driver function will overwrite the coefficients etc.
    */

    mod = lsq(list, dset, OLS, OPT_A);

    if (mod.errcode) {
        return mod;
    }

    lad_driver = get_plugin_function("lad_driver", &handle);

    if (lad_driver == NULL) {
	fputs(I_("Couldn't load plugin function\n"), stderr);
	mod.errcode = E_FOPEN;
	return mod;
    }

    (*lad_driver) (&mod, dset);
    close_plugin(handle);

    set_model_id(&mod);

    return mod;
}

/**
 * quantreg:
 * @tau: vector containing one or more quantile values, in the range 
 * 0.01 to 0.99.
 * @list: model specification: dependent var and regressors.
 * @dset: dataset struct.
 * @opt: may contain OPT_R for robust standard errors, 
 * OPT_I to produce confidence intervals.
 * @prn: gretl printing struct.
 *
 * Estimate the model given in @list using the method of 
 * quantile regression.
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL quantreg (const gretl_matrix *tau, const int *list, 
		DATASET *dset, gretlopt opt, PRN *prn)
{
    MODEL mod;
    void *handle;
    int (*rq_driver) (const gretl_matrix *, MODEL *, DATASET *,
		      gretlopt, PRN *);
    gretlopt olsopt = (OPT_A | OPT_M);

    /* Run an initial OLS to "set the model up" and check for errors.
       the driver function will overwrite the coefficients, etc.
       For now we make life easier by rejecting within-sample missing
       values (OPT_M).
    */

    if (opt & OPT_R) {
	olsopt |= OPT_R;
    }

    mod = lsq(list, dset, OLS, olsopt);

    if (mod.errcode) {
        return mod;
    }

    rq_driver = get_plugin_function("rq_driver", &handle);

    if (rq_driver == NULL) {
	fputs(I_("Couldn't load plugin function\n"), stderr);
	mod.errcode = E_FOPEN;
	return mod;
    }

    (*rq_driver) (tau, &mod, dset, opt, prn);
    close_plugin(handle);

    set_model_id(&mod);

    return mod;
}

/*
 * get_x12a_maxpd:
 *
 * Retrieve the highest data frequency handled by X-12-ARIMA,
 * which may vary depending on how the program was built.
 * This may be relevant when executing the gretl arima
 * command with gthe option to use X-12-ARIMA.
 */

int get_x12a_maxpd (void)
{
    static int n;

#ifdef WIN32
    if (n == 0) {
	char *sout = NULL;

	gretl_win32_grab_output(gretl_x12_arima(), &sout);

	if (sout != NULL) {
	    char *p = strstr(sout, "PSP = ");

	    if (p != NULL) {
		n = atoi(p + 6);
	    } 
	    free(sout);
	}

	if (n <= 0) {
	    n = 12;
	}
    }
#else
    if (n == 0) {
	const char *x12a = gretl_x12_arima();
	gchar *argv[] = { (gchar *) x12a, NULL };
	gchar *sout = NULL;
	int ok;

	ok = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
			  NULL, NULL, &sout, NULL,
			  NULL, NULL);
	if (ok && sout != NULL) {
	    char *p = strstr(sout, "PSP = ");

	    if (p != NULL) {
		n = atoi(p + 6);
	    } 
	}
	g_free(sout);

	if (n <= 0) {
	    n = 12;
	}
    }
#endif

    return n;
}

/**
 * arma:
 * @list: AR and MA orders, dependent variable, and any exogenous
 * regressors.
 * @pqlags: list giving specific non-seasonal AR/MA lags (or NULL).
 * @dset: dataset struct.
 * @opt: option flags.
 * @PRN: for printing details of iterations (or NULL). 
 *
 * Calculates ARMA estimates. By default the estimator is
 * exact ML, implemented via gretl's Kalman filter in
 * conjunction with the BFGS maximizer. Other options:
 * if @opt includes OPT_L we use L-BFGS-B rather than standard
 * BFGS; OPT_C (incompatible with OPT_L) calls for use of
 * conditional ML (BHHH algorithm); and OPT_X requests estimation via
 * X-12-ARIMA rather than native code.
 *
 * If @pqlags is non-NULL it should take the form of two
 * gretl sub-lists joined by #LISTSEP: the first sub-list holds
 * a set of AR lags and the second a list of MA lags. If only
 * AR lags are specified, a single list may be given; if only
 * MA lags are specified, use #LISTSEP with a null first sub-list.
 *
 * If the model specification does not include a constant
 * this is added automatically, unless @opt includes OPT_N
 * ("no constant").
 *
 * When the estimation method is native exact ML, two (mutually 
 * exclusive) flags in @opt may be used to control the estimator 
 * of the covariance matrix: OPT_G specifies use of the outer 
 * product of the gradient (OPG), while OPT_H specifies use of
 * the (numerical) Hessian. If neither of these flags is given, the
 * default is to use the Hessian by preference, but to fall back
 * to OPG if computation of the numerical Hessian fails. These
 * flags are ignored if estimation is not via native exact ML.
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL arma (const int *list, const int *pqlags, 
	    const DATASET *dset, gretlopt opt, 
	    PRN *prn)
{
    MODEL (*arma_model) (const int *, const int *,
			 const DATASET *, 
			 gretlopt, PRN *);
    MODEL (*arma_x12_model) (const int *, const int *,
			     const DATASET *, 
			     int, gretlopt, PRN *);
    MODEL armod;
    void *handle;
    int err = 0;

    gretl_model_init(&armod, dset);

    err = incompatible_options(opt, OPT_G | OPT_H);
    if (err) {
	armod.errcode = err;
	return armod;
    }	

    if (opt & OPT_X) {
	int pdmax = get_x12a_maxpd();

	if ((dset->t2 - dset->t1 + 1) > pdmax * 50) {
	    gretl_errmsg_sprintf(_("X-12-ARIMA can't handle more than %d observations.\n"
				   "Please select a smaller sample."), pdmax * 50);
	    armod.errcode = E_DATA;
	    return armod;
	}

	arma_x12_model = get_plugin_function("arma_x12_model", &handle);

	if (arma_x12_model == NULL) {
	    err = E_FOPEN;
	} else {
	    armod = (*arma_x12_model) (list, pqlags, dset, pdmax, opt, prn);
	}
    } else {
	arma_model = get_plugin_function("arma_model", &handle);
	if (arma_model == NULL) {
	    err = E_FOPEN;
	} else {
	    armod = (*arma_model) (list, pqlags, dset, opt, prn);
	}
    }

    if (err) {
	fputs(I_("Couldn't load plugin function\n"), stderr);
	armod.errcode = err;
	return armod;
    }

    close_plugin(handle);

    set_model_id(&armod);

    return armod;
} 

/**
 * garch:
 * @list: dependent variable plus arch and garch orders.
 * @dset: dataset struct.
 * @opt: can specify robust standard errors and VCV.
 * @prn: for printing details of iterations (or NULL).
 *
 * Calculate GARCH estimates.
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL garch (const int *list, DATASET *dset, gretlopt opt,
	     PRN *prn)
{
    MODEL mod;
    void *handle;
    PRN *myprn;
    MODEL (*garch_model) (const int *, DATASET *, PRN *,
			  gretlopt);

    gretl_error_clear();

    garch_model = get_plugin_function("garch_model", &handle);

    if (garch_model == NULL) {
	gretl_model_init(&mod, dset);
	mod.errcode = E_FOPEN;
	return mod;
    }

    if (opt & OPT_V) {
	myprn = prn;
    } else {
	myprn = NULL;
    }

    mod = (*garch_model) (list, dset, myprn, opt);

    close_plugin(handle);

    set_model_id(&mod);

    return mod;
} 

/**
 * mp_ols:
 * @list: specification of variables to use.
 * @dset: dataset struct.
 *
 * Estimate an OLS model using multiple-precision arithmetic
 * via the GMP library.
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL mp_ols (const int *list, DATASET *dset)
{
    void *handle = NULL;
    int (*mplsq)(const int *, const int *, const int *, 
		 DATASET *, MODEL *, 
		 gretlopt);
    MODEL mpmod;

    gretl_model_init(&mpmod, dset);

    mplsq = get_plugin_function("mplsq", &handle);
    if (mplsq == NULL) {
	mpmod.errcode = 1;
	return mpmod;
    }

    if (gretl_list_has_separator(list)) {
	int *base = NULL;
	int *poly = NULL;

	mpmod.errcode = gretl_list_split_on_separator(list, &base, &poly);
	if (mpmod.errcode == 0 && (base == NULL || poly == NULL)) {
	    mpmod.errcode = E_ARGS;
	} else {
	    mpmod.errcode = (*mplsq)(base, poly, NULL, dset,  
				     &mpmod, OPT_S);
	}
	free(base);
	free(poly);
    } else {
	mpmod.errcode = (*mplsq)(list, NULL, NULL, dset,  
				 &mpmod, OPT_S); 
    }

    close_plugin(handle);

    set_model_id(&mpmod);

    return mpmod;
}

static int check_panel_options (gretlopt opt)
{
    if ((opt & OPT_U) && (opt & OPT_W)) {
	/* can't specify random effects + weighted least squares */
	return E_BADOPT;
    } else if ((opt & OPT_T) && !(opt & OPT_W)) {
	/* iterate option requires weighted least squares option */
	return E_BADOPT;
    } else if ((opt & OPT_N) && !(opt & OPT_U)) {
	/* the Nerlove option requires random effects */
	return E_BADOPT;
    } else if ((opt & OPT_C) && !(opt & OPT_P)) {
	/* explicit cluster option only OK with pooled OLS */
	return E_BADOPT;
    } else if (incompatible_options(opt, OPT_B | OPT_U | OPT_P)) {
	/* mutually exclusive estimator requests */
	return E_BADOPT;
    }

    return 0;
}

/**
 * panel_model:
 * @list: regression list (dependent variable plus independent 
 * variables).
 * @dset: dataset struct.
 * @opt: can include OPT_Q (quiet estimation), OPT_S
 * (silent estimation), OPT_R (random effects model),
 * OPT_W (weights based on the error variance for the
 * respective cross-sectional units), OPT_I (iterate, only
 * available in conjunction with OPT_W).
 * @prn: printing struct (or NULL).
 *
 * Calculate estimates for a panel dataset, using fixed
 * effects (the default), random effects, or weighted
 * least squares based on the respective variances for the
 * cross-sectional units.
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL panel_model (const int *list, DATASET *dset,
		   gretlopt opt, PRN *prn)
{
    MODEL mod;

    if (check_panel_options(opt)) {
	gretl_model_init(&mod, dset);
	mod.errcode = E_BADOPT;
    } else if (opt & OPT_W) {
	mod = panel_wls_by_unit(list, dset, opt, prn);
    } else {
	mod = real_panel_model(list, dset, opt, prn);
    }

    return mod;
}

/**
 * ivreg:
 * @list: regression list; see the documentation for the "tsls"
 * command.
 * @dset: dataset struct.
 * @opt: option flags.
 *
 * Calculate IV estimates for a linear model, by default via two-stage 
 * least squares. The option flags can include OPT_Q for quiet
 * operation, and either OPT_L to specify estimation via Limited
 * Information Maximum Likelihood or OPT_G for estimation via
 * Generalized method of Moments.
 * 
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL ivreg (const int *list, DATASET *dset, gretlopt opt)
{
    MODEL mod;
    int err;

    gretl_error_clear();

    /* can't have both LIML and GMM options */
    err = incompatible_options(opt, OPT_G | OPT_L);

    if (!err) {
	/* two-step, iterate and weights options are GMM-only */
	err = option_prereq_missing(opt, OPT_T | OPT_I | OPT_W, OPT_G);
    }
    
    if (err) {
	gretl_model_init(&mod, dset);
	mod.errcode = err;
	return mod;
    }

    if (opt & OPT_L) {
	mod = single_equation_liml(list, dset, opt);
    } else if (opt & OPT_G) {
	mod = ivreg_via_gmm(list, dset, opt);
    } else {
	mod = tsls(list, dset, opt);
    }

    return mod;
}

/**
 * arbond_model:
 * @list: regression list.
 * @ispec: may contain additional instrument specification.
 * @dset: dataset struct.
 * @opt: may include OPT_D to include time dummies, 
 * OPT_H to transform the dependent variable via orthogonal
 * deviations rather than first differences, OPT_T for two-step
 * estimation, OPT_A to force production of asymptotic standard
 * errors rather than finite-sample corrected ones.
 * @prn: printing struct.
 *
 * Produces estimates of a dynamic panel-data model in
 * the manner of Arellano and Bond. See the documentation for
 * the "arbond" command in gretl for the construction of the
 * @list argument and also the syntax of @ispec.
 *
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL arbond_model (const int *list, const char *ispec,  
		    const DATASET *dset, gretlopt opt, 
		    PRN *prn)
{
    void *handle = NULL;
    MODEL (*arbond_estimate) (const int *, const char *, 
			      const DATASET *, gretlopt, PRN *);
    MODEL mod;

    gretl_model_init(&mod, dset);

    arbond_estimate = get_plugin_function("arbond_estimate", &handle);
    if (arbond_estimate == NULL) {
	mod.errcode = 1;
	return mod;
    }

    mod = (*arbond_estimate)(list, ispec, dset, opt, prn);

    close_plugin(handle);

    set_model_id(&mod);

    return mod;    
}

/**
 * dpd_model:
 * @list: regression list.
 * @laglist: list of specific lags of the dependent variable, or
 * NULL.
 * @ispec: may contain additional instrument specification.
 * @dset: dataset struct.
 * @opt: to be hooked up.
 * @prn: printing struct.
 *
 * This is at present a secret function, for testing only.
 *
 * Returns: a #MODEL struct, containing the estimates.
 */

MODEL dpd_model (const int *list, const int *laglist,
		 const char *ispec, const DATASET *dset, 
		 gretlopt opt, PRN *prn)
{
    void *handle = NULL;
    MODEL (*dpd_estimate) (const int *, const int *,
			   const char *, const DATASET *, 
			   gretlopt, PRN *);
    MODEL mod;

    gretl_model_init(&mod, dset);

    dpd_estimate = get_plugin_function("dpd_estimate", &handle);
    if (dpd_estimate == NULL) {
	mod.errcode = 1;
	return mod;
    }

    mod = (*dpd_estimate)(list, laglist, ispec, dset, opt, prn);

    close_plugin(handle);

    set_model_id(&mod);

    return mod;    
}

/**
 * anova:
 * @list: must contain the response and treatment variables.
 * @dset: dataset struct.
 * @opt: unused at present.
 * @prn: printing struct.
 * 
 * Does one-way or two-way Analysis of Variance (prints table and F-test).
 * 
 * Returns: 0 on success, non-zero on failure.
*/

int anova (const int *list, const DATASET *dset, 
	   gretlopt opt, PRN *prn)
{
    void *handle = NULL;
    int (*gretl_anova) (const int *, const DATASET *, 
			gretlopt, PRN *);
    int err;

    gretl_anova = get_plugin_function("gretl_anova", &handle);
    if (gretl_anova == NULL) {
	return 1;
    }

    err = (*gretl_anova)(list, dset, opt, prn);

    close_plugin(handle);

    return err;    
}


