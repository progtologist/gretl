/* for inclusion in interact.c: provides a mechanism for handling
   the fallout from the auto-generation of lags when an estimation
   command using the syntax of "foo(-1)" in the regression list,
   or "foo(-1 to -4)", etc.
*/

#ifndef LAGINFO_H
#define LAGINFO_H


#include <gretl/lib/libgretl.h>
#include <gretl/lib/monte_carlo.h>
#include <gretl/lib/var.h>
#include <gretl/lib/johansen.h>
#include <gretl/lib/gretl_func.h>
#include <gretl/lib/compat.h>
#include <gretl/lib/system.h>
#include <gretl/lib/forecast.h>
#include <gretl/lib/cmd_private.h>
#include <gretl/lib/libset.h>
#include <gretl/lib/uservar.h>
#include <gretl/lib/gretl_panel.h>
#include <gretl/lib/texprint.h>
#include <gretl/lib/gretl_xml.h>
#include <gretl/lib/gretl_string_table.h>
#include <gretl/lib/dbread.h>
#include <gretl/lib/gretl_foreign.h>
#include <gretl/lib/boxplots.h>
#include <gretl/lib/kalman.h>
#include <gretl/lib/flow_control.h>
#include <gretl/lib/libglue.h>
#include <gretl/lib/csvdata.h>
#ifdef USE_CURL
 #include <gretl/lib/gretl_www.h>
#endif

#include <errno.h>
#include <glib.h>

/* for the "shell" command */
#ifndef WIN32
# ifdef HAVE_PATHS_H
#  include <paths.h>
# endif
#endif

struct Laginfo_ {
    int *reflist;    /* list of distinct var for which we'll generate lags */
    int **lag_lists; /* list of lags to be generated, per var */
    int *srclist;    /* "shadow" of command list, with "source" IDs in place
            of lag vars IDs */
};

Laginfo *list_lag_info_new (void);

void list_lag_info_destroy (Laginfo *linfo);

void cmd_lag_info_destroy (CMD *cmd);

/* unlike gretl_list_separator_position(), we start the check
   at position 1 here */

int laglist_sep_pos (const int *list);

/* retrieve list of lags for variable v */

const int *get_lag_list_by_varnum (int v, const Laginfo *linfo);

/* add a specific lag to the record of lags for a given
   variable; insert a list separator first, if needed
*/

int
add_lag_to_laglist (int srcv, int lag, int spos, Laginfo *linfo);

int
print_lags_by_varnum (int v, const Laginfo *linfo,
              const DATASET *dset,
              int gotsep, PRN *prn);

/* below: FIXME case of i == 1 ?? (auto-lagged dependent var) */

int
is_auto_generated_lag (int i, const int *cmdlist, const Laginfo *linfo);

/* Determine whether the var that appears at position i in
   cmd->list is the first lag of any variable that appears in
   the command list: if so, it will be used as an 'anchor' for echoing
   other lags of the same variable.  Complication: 'first' really
   means 'first within a given sublist' if the command list contains
   sublists.  (Conversely, if a lag var is *not* 'first' in this
   sense it should not be echoed separately, since it will have
   benn handled already.)
*/

int
is_first_lag (int i, const int *cmdlist, int sep, const Laginfo *linfo, int *src);



#endif // LAGINFO_H
