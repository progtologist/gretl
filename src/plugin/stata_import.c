/*
   Reader for Stata .dta files, versions 5.0 to 11.

   Based on stataread.c from the GNU R "foreign" package with the 
   following original info:

     * $Id: stata_import.c,v 1.56 2013/12/31 22:47:15 allin Exp $
  
     (c) 1999, 2000, 2001, 2002 Thomas Lumley. 
     2000 Saikat DebRoy

     The format of Stata files is documented under 'file formats' 
     in the Stata manual.

     This code currently does not make use of the print format information in 
     a .dta file (except for dates). It cannot handle files with 'int'
     'float' or 'double' that differ from IEEE 4-byte integer, 4-byte
     real and 8-byte real respectively: it's not clear whether such files
     can exist.

     Versions of Stata before 4.0 used different file formats.

  This version was fairly substantially modified for gretl 
  by Allin Cottrell, July 2005, then modified again in August
  2009 to support "format 114" .dta files as written by Stata
  10 and 11.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <glib.h>

#include <gretl/lib/libgretl.h>
#include <gretl/lib/version.h>
#include <gretl/lib/gretl_string_table.h>
#include <gretl/lib/swap_bytes.h>

#ifdef WORDS_BIGENDIAN
# define HOST_ENDIAN G_BIG_ENDIAN
#else
# define HOST_ENDIAN G_LITTLE_ENDIAN
#endif

/* Stata versions */
#define VERSION_5   0x69
#define VERSION_6     'l'
#define VERSION_7   0x6e
#define VERSION_7SE  111
#define VERSION_8    113
#define VERSION_10   114
#define VERSION_12   115

/* Stata format constants */
#define STATA_STRINGOFFSET 0x7f
#define STATA_FLOAT    'f'
#define STATA_DOUBLE   'd'
#define STATA_LONG     'l'
#define STATA_INT      'i'
#define STATA_BYTE     'b'

/* Stata SE format constants */
#define STATA_SE_STRINGOFFSET 0
#define STATA_SE_FLOAT    254
#define STATA_SE_DOUBLE   255
#define STATA_SE_LONG     253
#define STATA_SE_INT      252
#define STATA_SE_BYTE     251

/* see http://www.stata.com/help.cgi?dta */
#define STATA_FLOAT_MAX  1.701e+38
#define STATA_DOUBLE_MAX 8.988e+307
#define STATA_LONG_MAX   2147483620
#define STATA_INT_MAX    32740

/* values from R's stataread.c -- these were labeled "*NA" */
#if 0
# define STATA_FLOAT_CUT  pow(2.0, 127)
# define STATA_DOUBLE_CUT pow(2.0, 1023)
# define STATA_LONG_CUT   2147483647
#endif
#define STATA_INT_CUT    32767

/* Stata missing value codes: see http://www.stata.com/help.cgi?dta */
#define STATA_FLOAT_NA(x)  (x > STATA_FLOAT_MAX)
#define STATA_DOUBLE_NA(x) (x > STATA_DOUBLE_MAX)
#define STATA_LONG_NA(i)   (i > STATA_LONG_MAX)
#define STATA_INT_NA(i)    (i > STATA_INT_MAX)

#define STATA_BYTE_NA(b,v) ((v<8 && b==127) || b>=101)

#define NA_INT -999

/* it's convenient to have these as file-scope globals */
static int stata_version;
static int stata_SE;
static int stata_endian;
static int swapends;

static void bin_error (int *err)
{
    fputs("binary read error\n", stderr);
    *err = 1;
}

/* actually an int (4-byte signed int) */

static int stata_read_long (FILE *fp, int naok, int *err)
{
    int i;

    if (fread(&i, sizeof i, 1, fp) != 1) {
	bin_error(err);
	return NA_INT;
    }

    if (swapends) {
	reverse_int(i);
    }

    return (STATA_LONG_NA(i) & !naok)? NA_INT : i;
}

static int stata_read_signed_byte (FILE *fp, int naok, int *err)
{ 
    signed char b;
    int ret;

    if (fread(&b, 1, 1, fp) != 1) {
	bin_error(err);
	ret = NA_INT;
    } else {
	ret = (int) b;

	if (!naok && STATA_BYTE_NA(b, stata_version)) {
	    ret = NA_INT;
	}
    }

    return ret;
}

static int stata_read_byte (FILE *fp, int *err)
{ 
    unsigned char u;

    if (fread(&u, 1, 1, fp) != 1) {
	bin_error(err);
	return NA_INT;
    }

    return (int) u;
}

/* actually a short (2-byte signed int) */

static int stata_read_int (FILE *fp, int naok, int *err)
{
    unsigned first, second;
    int s;
	
    first = stata_read_byte(fp, err);
    second = stata_read_byte(fp, err);

    if (stata_endian == G_BIG_ENDIAN) {
	s = (first << 8) | second;
    } else {
	s = (second << 8) | first;
    }

    if (s > STATA_INT_CUT) { 
	/* ?? */
	s -= 65536;
    }

    return (STATA_INT_NA(s) && !naok)? NA_INT : s;
}

static double stata_read_double (FILE *fp, int *err)
{
    double d;

    if (fread(&d, sizeof d, 1, fp) != 1) {
	bin_error(err);
    }

    if (swapends) {
	reverse_double(d);
    }

    return (STATA_DOUBLE_NA(d))? NADBL : d;
}

static double stata_read_float (FILE *fp, int *err)
{
    float f;

    if (fread(&f, sizeof f, 1, fp) != 1) {
	bin_error(err);
    }

    if (swapends) {
	reverse_float(f);
    }

    return (STATA_FLOAT_NA(f))? NADBL : (double) f;
}

static void stata_read_string (FILE *fp, int nc, char *buf, int *err)
{
    if (fread(buf, 1, nc, fp) != nc) {
	bin_error(err);
    }
}

static int 
stata_get_version_and_namelen (unsigned char u, int *vnamelen)
{
    int err = 0;

    switch (u) {
    case VERSION_5:
        stata_version = 5;
	*vnamelen = 8;
	break;
    case VERSION_6:
        stata_version = 6;
	*vnamelen = 8;
	break;
    case VERSION_7:
	stata_version = 7;
	*vnamelen = 32;
	break;
    case VERSION_7SE:
	stata_version = 7;
	stata_SE = 1;
	*vnamelen = 32; 
	break;
    case VERSION_8:
	stata_version = 8; /* versions >= 8 automatically use 'SE' format */
	stata_SE = 1;
	*vnamelen = 32; 
	break;
    case VERSION_10:
	stata_version = 10;
	stata_SE = 1;
	*vnamelen = 32; 
	break;
    case VERSION_12:
	stata_version = 12;
	stata_SE = 1;
	*vnamelen = 32; 
	break;
    default:
        err = 1;
    }

    return err;
}

static int stata_get_endianness (FILE *fp, int *err)
{
    int i = (int) stata_read_byte(fp, err);

    return (i == 0x01)? G_BIG_ENDIAN : G_LITTLE_ENDIAN;
}

#define stata_type_float(t)  ((stata_SE && t == STATA_SE_FLOAT)  || t == STATA_FLOAT)
#define stata_type_double(t) ((stata_SE && t == STATA_SE_DOUBLE) || t == STATA_DOUBLE)
#define stata_type_long(t)   ((stata_SE && t == STATA_SE_LONG)   || t == STATA_LONG)
#define stata_type_int(t)    ((stata_SE && t == STATA_SE_INT)    || t == STATA_INT)
#define stata_type_byte(t)   ((stata_SE && t == STATA_SE_BYTE)   || t == STATA_BYTE)
#define stata_type_string(t) ((stata_SE && t <= 244) || t >= STATA_STRINGOFFSET)

static int check_variable_types (FILE *fp, int *types, int nvar, int *nsv,
				 PRN *prn)
{
    int i, err = 0;

    *nsv = 0;

    for (i=0; i<nvar && !err; i++) {
   	unsigned char u = stata_read_byte(fp, &err);

	types[i] = u;
	if (stata_type_float(u) || stata_type_double(u)) {
	    pprintf(prn, "variable %d: float type\n", i+1);
	} else if (stata_type_long(u)) {
	    pprintf(prn, "variable %d: long type\n", i+1);
	} else if (stata_type_int(u)) {
	    pprintf(prn, "variable %d: int type\n", i+1);
	} else if (stata_type_byte(u)) {
	    pprintf(prn, "variable %d: byte type\n", i+1);
	} else if (stata_type_string(u)) {
	    pprintf(prn, "variable %d: string type\n", i+1);
	    *nsv += 1;
	} else {
	    pputs(prn, _("unknown data type"));
	    pputc(prn, '\n');
	    err = 1;
	}
    }

    return err;
}

/* mechanism for handling (coding) non-numeric variables */

static gretl_string_table *
dta_make_string_table (int *types, int nvar, int ncols)
{
    gretl_string_table *st;
    int *list;
    int i, j;

    list = gretl_list_new(ncols);
    if (list == NULL) {
	return NULL;
    }

    j = 1;
    for (i=0; i<nvar && j<=list[0]; i++) {
	if (!stata_type_float(types[i]) &&
	    !stata_type_double(types[i]) &&
	    !stata_type_long(types[i]) &&
	    !stata_type_int(types[i]) &&
	    !stata_type_byte(types[i])) {
	    list[j++] = i + 1;
	}
    }

    st = gretl_string_table_new(list);

    free(list);

    return st;
}

static gchar *recode_stata_string (const char *s)
{
    const gchar *cset;
    gchar *tr = NULL;
    gsize bw;

    if (!g_get_charset(&cset)) {
	/* try recoding from current locale */
	tr = g_locale_to_utf8(s, -1, NULL, &bw, NULL);
    }

    if (tr == NULL) {
	/* wild guess: try Windows CP 1252? */
	tr = g_convert(s, -1, "UTF-8", "CP1252", NULL, &bw, NULL);
    }

    return tr;
}

static void
save_dataset_info (DATASET *dinfo, const char *label, const char *stamp)
{
    int dlen = strlen(stamp);
    gchar *tr = NULL;

    if (*label != '\0') {
	if (g_utf8_validate(label, -1, NULL)) {
	    tr = g_strdup(label);
	} else {
	    tr = recode_stata_string(label);
	}
    }

    if (tr != NULL) {
	dlen += strlen(tr);
    }

    if (dlen > 0) {
	dinfo->descrip = malloc(dlen + 2);
    }

    if (dinfo->descrip != NULL) {
	*dinfo->descrip = '\0';
	if (tr != NULL) {
	    strcat(dinfo->descrip, tr);
	    strcat(dinfo->descrip, "\n");
	} 
	strcat(dinfo->descrip, stamp);
    } 

    g_free(tr);
}

static int try_fix_varname (char *name)
{
    char test[VNAMELEN];
    int err = 0;

    *test = 0;

    if (*name == '_') {
	strcat(test, "x");
	strncat(test, name, VNAMELEN - 2);
    } else {
	strncat(test, name, VNAMELEN - 2);
	strcat(test, "1");
    }
    
    err = check_varname(test);
    if (!err) {
	fprintf(stderr, "Warning: illegal name '%s' changed to '%s'\n",
		name, test);
	strcpy(name, test);
    } else {
	/* get the right error message in place */
	check_varname(name);
    }

    return err;
}

/* use Stata's "date formats" to reconstruct time series information
   FIXME: add recognition for daily data too? 
   (Stata dates are all zero at the start of 1960.)
*/

static int set_time_info (int t1, int pd, DATASET *dinfo)
{
    int yr, mo, qt;

    *dinfo->stobs = '\0';

    if (pd == 12) {
	yr = (t1 / 12) + 1960;
	mo = t1 % 12 + 1;
	sprintf(dinfo->stobs, "%d:%02d", yr, mo);
    } else if (pd == 4) {
	yr = (t1 / 4) + 1960;
	qt = t1 % 4 + 1;
	sprintf(dinfo->stobs, "%d:%d", yr, qt);
    } else {
	yr = t1 + 1960;
	if (yr > 2050) {
	    ; /* Can't be a year? (FIXME: how did we get here?) */
	} else {
	    sprintf(dinfo->stobs, "%d", yr);
	}
    }

    dinfo->pd = pd;

    if (*dinfo->stobs != '\0') {
	printf("starting obs seems to be %s\n", dinfo->stobs);
	dinfo->structure = TIME_SERIES;
	dinfo->sd0 = get_date_x(dinfo->pd, dinfo->stobs);
    } 

    return 0;
}

static int dta_value_labels_setup (PRN **pprn, gretl_string_table **pst)
{
    int err = 0;

    *pprn = gretl_print_new(GRETL_PRINT_BUFFER, &err);

    if (*pprn != NULL) {
	if (*pst == NULL) {
	    *pst = gretl_string_table_new(NULL);
	    if (*pst == NULL) {
		gretl_print_destroy(*pprn);
		*pprn = NULL;
	    }
	}
    }

    return err;
}

static int push_label_info (int **pv, char ***pS, int v, const char *lname)
{
    int n = (*pv == NULL)? 0 : (*pv)[0];
    int err = 0;

    if (gretl_list_append_term(pv, v) == NULL) {
	err = E_ALLOC;
    } else {
	err = strings_array_add(pS, &n, lname);
    }

    return err;
}

static int label_array_header (const int *list, char **names, 
			       const char *lname, const DATASET *dset,
			       PRN *prn)
{
    int i, v = 0, n = 0;

    for (i=1; i<=list[0]; i++) {
	if (!strcmp(names[i-1], lname)) {
	    v = list[i];
	    n++;
	}
    }

    if (n == 0) {
	return 0;
    }

    if (n == 1) {
	pprintf(prn, "\nValue -> label mappings for variable %d (%s)\n", 
		v, dset->varname[v]);
    } else {
	pprintf(prn, "\nValue -> label mappings for the following %d variables\n", n);
	for (i=1; i<=list[0]; i++) {
	    if (!strcmp(names[i-1], lname)) {
		v = list[i];
		pprintf(prn, " %3d (%s)\n", v, dset->varname[v]);
	    }
	}
    }

    return 1;
}

/* when printing stata format strings, make sure we double any
   '%' characters */

static void print_var_format (int k, const char *s, PRN *prn)
{
    char tmp[64];
    int i = 0;

    while (*s) {
	tmp[i++] = *s;
	if (*s == '%') {
	    tmp[i++] = '%';
	}
	s++;
    }

    tmp[i] = '\0';
    pprintf(prn, "variable %d: format = '%s'\n", k, tmp);    
}

static int read_dta_data (FILE *fp, DATASET *dset,
			  gretl_string_table **pst, int namelen,
			  int *nvread, PRN *prn, PRN *vprn)
{
    int i, j, t, clen;
    int labellen, nlabels, totlen;
    int fmtlen;
    int nvar = dset->v - 1, nsv = 0;
    int soffset, pd = 0, tnum = -1;
    char label[81], c50[50], aname[33];
    int *types = NULL;
    int *lvars = NULL;
    char **lnames = NULL;
    char strbuf[256];
    char *txt = NULL; 
    int *off = NULL;
    int st_err = 0;
    int err = 0;

    labellen = (stata_version == 5)? 32 : 81;
    fmtlen = (stata_version < 10)? 12 : 49;
    soffset = (stata_SE)? STATA_SE_STRINGOFFSET : STATA_STRINGOFFSET;
    *nvread = nvar;

    pprintf(vprn, "Max length of labels = %d\n", labellen);

    /* dataset label - zero terminated string */
    stata_read_string(fp, labellen, label, &err);
    pprintf(vprn, "dataset label: '%s'\n", label);

    /* file creation time - zero terminated string */
    stata_read_string(fp, 18, c50, &err);  
    pprintf(vprn, "timestamp: '%s'\n", c50);

    if (*label != '\0' || *c50 != '\0') {
	save_dataset_info(dset, label, c50);
    }
  
    /** read variable descriptors **/
    
    /* types */

    types = malloc(nvar * sizeof *types);
    if (types == NULL) {
	return E_ALLOC;
    }

    err = check_variable_types(fp, types, nvar, &nsv, vprn);
    if (err) {
	free(types);
	return err;
    }

    if (nsv > 0) {
	/* we have 1 or more non-numeric variables */
	*pst = dta_make_string_table(types, nvar, nsv);
    }

    /* variable names */
    for (i=0; i<nvar && !err; i++) {
        stata_read_string(fp, namelen + 1, aname, &err);
	/* try to fix possible bad encoding */
	iso_to_ascii(aname);
	pprintf(vprn, "variable %d: name = '%s'\n", i+1, aname);
	if (check_varname(aname) && try_fix_varname(aname)) {
	    err = 1;
	} else {
	    strncat(dset->varname[i+1], aname, VNAMELEN - 1);
	}
    }

    /* sortlist -- not relevant */
    for (i=0; i<2*(nvar+1) && !err; i++) {
        stata_read_byte(fp, &err);
    }
    
    /* format list (use it to identify date variables?) */
    for (i=0; i<nvar && !err; i++){
        stata_read_string(fp, fmtlen, c50, &err);
	if (stata_type_string(types[i])) {
	    /* the format is of no interest */
	    continue;
	}
	if (*c50 != '\0' && c50[strlen(c50)-1] != 'g') {
	    print_var_format(i+1, c50, vprn);
	    if (!strcmp(c50, "%tm")) {
		pd = 12;
		tnum = i;
	    } else if (!strcmp(c50, "%tq")) {
		pd = 4;
		tnum = i;
	    } else if (!strcmp(c50, "%ty")) {
		pd = 1;
		tnum = i;
	    }
	}
    }

    /* "value labels": these are stored as the names of label formats, 
       which are themselves stored later in the file */
    for (i=0; i<nvar && !err; i++) {
        stata_read_string(fp, namelen + 1, aname, &err);
	if (*aname != '\0' && !st_err) {
	    pprintf(vprn, "variable %d: \"value label\" = '%s'\n", i+1, aname);
	    st_err = push_label_info(&lvars, &lnames, i+1, aname);
	}
    }

    /* variable descriptive labels */
    for (i=0; i<nvar && !err; i++) {
	stata_read_string(fp, labellen, label, &err);
	if (*label != '\0') {
	    pprintf(vprn, "variable %d: label = '%s'\n", i+1, label);
	    if (g_utf8_validate(label, -1, NULL)) {
		series_set_label(dset, i+1, label);
	    } else {
		gchar *tr = recode_stata_string(label);

		if (tr != NULL) {
		    series_set_label(dset, i+1, tr);
		    g_free(tr);
		}
	    } 
	}
    }

    /* variable 'characteristics' -- not handled */
    if (!err) {
	while (stata_read_byte(fp, &err)) {
	    if (stata_version >= 7) { /* manual is wrong here */
		clen = stata_read_long(fp, 1, &err);
	    } else {
		clen = stata_read_int(fp, 1, &err);
	    }
	    for (i=0; i<clen; i++) {
		stata_read_signed_byte(fp, 1, &err);
	    }
	}
	if (stata_version >= 7) {
	    clen = stata_read_long(fp, 1, &err);
	} else {
	    clen = stata_read_int(fp, 1, &err);
	}
	if (clen != 0) {
	    fputs(_("something strange in the file\n"
		    "(Type 0 characteristic of nonzero length)"), stderr);
	    fputc('\n', stderr);
	}
    }

    /* actual data values */
    for (t=0; t<dset->n && !err; t++) {
	for (i=0; i<nvar && !err; i++) {
	    int ix, v = i + 1;

	    dset->Z[v][t] = NADBL; 

	    if (stata_type_float(types[i])) {
		dset->Z[v][t] = stata_read_float(fp, &err);
	    } else if (stata_type_double(types[i])) {
		dset->Z[v][t] = stata_read_double(fp, &err);
	    } else if (stata_type_long(types[i])) {
		ix = stata_read_long(fp, 0, &err);
		dset->Z[v][t] = (ix == NA_INT)? NADBL : ix;
	    } else if (stata_type_int(types[i])) {
		ix = stata_read_int(fp, 0, &err);
		dset->Z[v][t] = (ix == NA_INT)? NADBL : ix;
	    } else if (stata_type_byte(types[i])) {
		ix = stata_read_signed_byte(fp, 0, &err);
		dset->Z[v][t] = (ix == NA_INT)? NADBL : ix;
	    } else {
		clen = types[i] - soffset;
		if (clen > 255) {
		    clen = 255;
		} 
		stata_read_string(fp, clen, strbuf, &err);
		strbuf[clen] = 0;
#if 0
		fprintf(stderr, "Z[%d][%d] = '%s'\n", v, t, strbuf);
#endif
		if (!err && *strbuf != '\0' && strcmp(strbuf, ".") && *pst != NULL) {
		    ix = gretl_string_table_index(*pst, strbuf, v, 0, prn);
		    if (ix > 0) {
			dset->Z[v][t] = ix;
			if (t == 0) {
			    series_set_discrete(dset, v, 1);
			}
		    }	
		}
	    }

	    if (i == tnum && t == 0 && !err) {
		set_time_info((int) dset->Z[v][t], pd, dset);
	    }
	}
    }

    /* value labels */

    if (!err && !st_err && lvars != NULL && stata_version > 5) {
	PRN *st_prn = NULL;
	const char *vlabel;
	double *level;
	
	for (j=0; j<nvar; j++) {
	    /* first int not needed, use fread directly to trigger EOF */
	    fread((int *) aname, sizeof(int), 1, fp);
	    if (feof(fp)) {
		pprintf(vprn, "breaking on feof\n");
		break;
	    }

	    stata_read_string(fp, namelen + 1, aname, &err);
	    pprintf(vprn, "labels %d: name = '%s'\n", j, aname);

	    /* padding */
	    stata_read_byte(fp, &err);
	    stata_read_byte(fp, &err);
	    stata_read_byte(fp, &err);

	    nlabels = stata_read_long(fp, 1, &err);
	    totlen = stata_read_long(fp, 1, &err);

	    if (nlabels <= 0 || totlen <= 0) {
		break;
	    }

	    if (st_prn == NULL) {
		st_err = dta_value_labels_setup(&st_prn, pst);
		if (st_err) {
		    break;
		}
	    }

	    off = malloc(nlabels * sizeof *off);
	    if (off == NULL) {
		st_err = E_ALLOC;
		break;
	    }

	    level = malloc(nlabels * sizeof *level);
	    if (level == NULL) {
		free(off);
		st_err = E_ALLOC;
		break;
	    }

	    label_array_header(lvars, lnames, aname, dset, st_prn);

	    for (i=0; i<nlabels && !err; i++) {
		off[i] = stata_read_long(fp, 1, &err);
	    }

	    for (i=0; i<nlabels && !err; i++) {
		level[i] = (double) stata_read_long(fp, 0, &err);
		pprintf(vprn, " level %d = %g\n", i, level[i]);
	    }

	    txt = calloc(totlen, 1);
	    if (txt == NULL) {
		free(off);
		free(level);
		st_err = E_ALLOC;
		break;
	    }	    

	    stata_read_string(fp, totlen, txt, &err);

	    for (i=0; i<nlabels; i++) {
		vlabel = txt + off[i];
		pprintf(vprn, " label %d = '%s'\n", i, vlabel);
		if (g_utf8_validate(vlabel, -1, NULL)) {
		    pprintf(st_prn, "%10g -> '%s'\n", level[i], vlabel);
		} else {
		    gchar *tr = recode_stata_string(vlabel);

		    if (tr != NULL) {
			pprintf(st_prn, "%10g -> '%s'\n", level[i], tr);
			g_free(tr);
		    } else {
			pprintf(st_prn, "%10g -> 'unknown'\n", level[i]);
		    }
		}
	    }

	    free(off);
	    free(level);
	    free(txt);
	}

	if (st_prn != NULL) {
	    if (!st_err) {
		gretl_string_table_add_extra(*pst, st_prn);
	    }
	    gretl_print_destroy(st_prn);
	}
    }

    free(types);

    if (lvars != NULL) {
	strings_array_free(lnames, lvars[0]);
	free(lvars);
    }

    return err;
}

static int parse_dta_header (FILE *fp, int *namelen, int *nvar, int *nobs,
			     PRN *prn, PRN *vprn)
{
    unsigned char u;
    int err = 0;
    
    u = stata_read_byte(fp, &err); /* release version */

    if (!err) {
	err = stata_get_version_and_namelen(u, namelen);
    }

    if (err) {
	fputs("not a Stata version 5-12 .dta file\n", stderr);
	return err;
    } 

    pprintf(prn, "Stata file version %d\n", stata_version);

    /* these are file-scope globals */
    stata_endian = stata_get_endianness(fp, &err);
    swapends = stata_endian != HOST_ENDIAN;

    stata_read_byte(fp, &err);              /* filetype -- junk */
    stata_read_byte(fp, &err);              /* padding */
    *nvar = stata_read_int(fp, 1, &err);    /* number of variables */
    *nobs = stata_read_long(fp, 1, &err);   /* number of observations */

    if (!err && (*nvar <= 0 || *nobs <= 0)) {
	err = 1;
    }

    if (!err && vprn != NULL) {
	pprintf(vprn, "endianness: %s\n", (stata_endian == G_BIG_ENDIAN)? 
		"big" : "little");
	pprintf(vprn, "number of variables = %d\n", *nvar);
	pprintf(vprn, "number of observations = %d\n", *nobs);
	pprintf(vprn, "length of varnames = %d\n", *namelen);
    }

    return err;
}

int dta_get_data (const char *fname, DATASET *dset,
		  gretlopt opt, PRN *prn)
{
    int namelen = 0, nobs = 0;
    int nvar = 0, nvread = 0;
    FILE *fp;
    DATASET *newset = NULL;
    gretl_string_table *st = NULL;
    PRN *vprn = prn;
    int err = 0;

    if (sizeof(double) != 8 || sizeof(int) != 4 || sizeof(float) != 4) {
	pputs(prn, _("cannot read Stata .dta on this platform"));
	return E_DATA;
    }

    fp = gretl_fopen(fname, "rb");
    if (fp == NULL) {
	return E_FOPEN;
    }

    if (opt & OPT_Q) {
	vprn = NULL;
    }

    err = parse_dta_header(fp, &namelen, &nvar, &nobs, prn, vprn);
    if (err) {
	pputs(prn, _("This file does not seem to be a valid Stata data file"));
	fclose(fp);
	return E_DATA;
    }

    newset = datainfo_new();
    if (newset == NULL) {
	pputs(prn, _("Out of memory\n"));
	fclose(fp);
	return E_ALLOC;
    }

    newset->v = nvar + 1;
    newset->n = nobs;
    dataset_obs_info_default(newset);

    err = start_new_Z(newset, 0);
    if (err) {
	pputs(prn, _("Out of memory\n"));
	free_datainfo(newset);
	fclose(fp);
	return E_ALLOC;
    }	

    err = read_dta_data(fp, newset, &st, namelen, &nvread, prn, vprn);

    if (err) {
	destroy_dataset(newset);
	if (st != NULL) {
	    gretl_string_table_destroy(st);
	}	
    } else {
	int merge = (dset->Z != NULL);
	int nvtarg = newset->v - 1;

	if (nvread < nvtarg) {
	    dataset_drop_last_variables(newset, nvtarg - nvread);
	}
	
	if (fix_varname_duplicates(newset)) {
	    pputs(prn, _("warning: some variable names were duplicated\n"));
	}

	if (st != NULL) {
	    gretl_string_table_print(st, newset, fname, prn);
	    gretl_string_table_destroy(st);
	}

	err = merge_or_replace_data(dset, &newset, opt, prn);
    
	if (!err && !merge) {
	    dataset_add_import_info(dset, fname, GRETL_DTA);
	}
    }

    fclose(fp);

    return err;
}  
