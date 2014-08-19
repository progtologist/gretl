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

/* callbacks.c for gretl */

#include <gretl/gui2/gretl.h>
#include <gretl/gui2/selector.h>
#include <gretl/gui2/session.h>
#include <gretl/gui2/database.h>
#include <gretl/gui2/datafiles.h>
#include <gretl/gui2/textbuf.h>
#include <gretl/gui2/textutil.h>
#include <gretl/gui2/dlgutils.h>
#include <gretl/gui2/fileselect.h>
#include <gretl/gui2/ssheet.h>
#include <gretl/gui2/treeutils.h>
#include <gretl/gui2/datawiz.h>
#include <gretl/gui2/winstack.h>

#include <gretl/lib/boxplots.h>

static void doubleclick_action (windata_t *vwin)
{
    switch (vwin->role) {
    case MAINWIN:
	if (dataset != NULL && dataset->n > 0) {
	    display_var();
	}
	break;
    case TEXTBOOK_DATA:
	browser_open_data(NULL, vwin);
	break;
    case PS_FILES:
	browser_open_ps(NULL, vwin);
	break;
    case FUNC_FILES:
	browser_call_func(NULL, vwin);
	break;
    case NATIVE_DB:
	open_db_index(NULL, vwin); 
	break;
    case REMOTE_DB:
	open_remote_db_index(NULL, vwin);
	break;
    case NATIVE_SERIES:
    case RATS_SERIES:
    case PCGIVE_SERIES:
    case REMOTE_SERIES:
	display_db_series(vwin);
	break;
    default:
	break;
    }
}

void listbox_select_row (GtkTreeSelection *selection, gpointer data)
{
    windata_t *win = (windata_t *) data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath *path;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
	return;
    }

    path = gtk_tree_model_get_path(model, &iter);
    win->active_var = tree_path_get_row_number(path);
    gtk_tree_path_free(path);
}

gint listbox_double_click (GtkWidget *widget, GdkEventButton *event,
			   windata_t *win)
{
    if (event != NULL && event->type == GDK_2BUTTON_PRESS 
	&& event->button == 1) {
	doubleclick_action(win);
    }
    return FALSE;
}

gboolean listbox_drag (GtkWidget *listbox, GdkEventMotion *event,
		       gpointer data)
{
    gint x, y;
    GdkModifierType state;
    GtkTreeView *view = GTK_TREE_VIEW(listbox);
    GtkTreePath *path;

    if (event->is_hint) {
        gdk_window_get_pointer(event->window, &x, &y, &state);
    } else {
        x = event->x;
        y = event->y;
        state = event->state;
    }

    if ((state & GDK_BUTTON1_MASK) && 
	gtk_tree_view_get_path_at_pos(view, x, y, &path, 
				      NULL, NULL, NULL)) {
	GtkTreeSelection *select = NULL;
	GtkTreePath *anchor_path = NULL;
	gchar *anchor_id = NULL;
	gint row;
	int anchor;
	static gint lastrow;

	anchor = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(listbox), 
						   "active_row"));
	row = tree_path_get_row_number(path);

	select = gtk_tree_view_get_selection(view);
	if (select == NULL) {
	    return FALSE;
	}
	anchor_id = g_strdup_printf("%d", anchor);
	anchor_path = gtk_tree_path_new_from_string(anchor_id);
	g_free(anchor_id);

	if (row != lastrow) {
	    gtk_tree_selection_unselect_all(select);
	    gtk_tree_selection_select_range(select, anchor_path,
					    path);
	}

	gtk_tree_path_free(path);
	gtk_tree_path_free(anchor_path);

	lastrow = row;
    }

    return FALSE;
}

struct open_data_code {
    int c;
    const gchar *s;
};

struct open_data_code open_data_codes[] = {
    { OPEN_DATA,       "OpenData" },    
    { APPEND_DATA,     "AppendData" },
    { OPEN_MARKERS,    "AddMarkers" },
    { OPEN_RATS_DB,    "RATSDB" },
    { OPEN_PCGIVE_DB,  "PcGiveDB" },
    { 0, NULL }
};

static int open_data_code (const gchar *s)
{
    int i;

    for (i=0; open_data_codes[i].s != NULL; i++) {
	if (!strcmp(s, open_data_codes[i].s)) {
	    return open_data_codes[i].c;
	}
    }

    return 0;
}

void open_data (GtkAction *action)
{
    if (!dataset_locked()) {
	int code = open_data_code(gtk_action_get_name(action));

	file_selector(code, FSEL_DATA_NONE, NULL);
    }
}

void file_save (windata_t *vwin, int ci)
{
    switch (ci) {
    case SAVE_OUTPUT:
    case SAVE_CONSOLE:
    case SAVE_SCRIPT:
    case SAVE_GP_CMDS:
    case SAVE_R_CMDS:
    case SAVE_DATA:
    case SAVE_DATA_AS:
	file_selector(ci, FSEL_DATA_VWIN, vwin);
	break;
    case EXPORT_CSV:
    case EXPORT:
	data_export_selection_wrapper(ci);
	break;
    case SAVE_FUNCTIONS:
	functions_selection_wrapper();
	break;
    case SAVE_TEX:
    case SAVE_TEXT:
	file_selector(ci, FSEL_DATA_MISC, vwin->data);
	break;
    default:
	dummy_call();
    }
}

static int fsave_code (const gchar *s)
{
    if (!strcmp(s, "SaveDataAs"))
	return SAVE_DATA_AS;
    if (!strcmp(s, "ExportData"))
	return EXPORT;
    if (!strcmp(s, "NewGfn"))
	return SAVE_FUNCTIONS;

    return SAVE_DATA;
}

void fsave_callback (GtkAction *action, gpointer p)
{
    const gchar *s = gtk_action_get_name(action);
    int ci = fsave_code(s);

    file_save(p, ci);
}

void dummy_call (void)
{
    errbox(_("Sorry, this item not yet implemented!"));
}

static int model_action_code (GtkAction *action)
{
    const gchar *s = gtk_action_get_name(action);
    int ci = gretl_command_number(s);

    if (ci == 0) {
	/* look up "GUI special" */
	if (!strcmp(s, "PANEL_WLS"))
	    ci = PANEL_WLS;
	else if (!strcmp(s, "PANEL_B"))
	    ci = PANEL_B;
	else if (!strcmp(s, "VLAGSEL"))
	    ci = VLAGSEL;
	else if (!strcmp(s, "blogit"))
	    ci = LOGIT;
	else if (!strcmp(s, "ologit"))
	    ci = OLOGIT;
	else if (!strcmp(s, "mlogit"))
	    ci = MLOGIT;
	else if (!strcmp(s, "bprobit"))
	    ci = PROBIT;
	else if (!strcmp(s, "oprobit"))
	    ci = OPROBIT;
	else if (!strcmp(s, "reprobit"))
	    ci = REPROBIT;
	else if (!strcmp(s, "iv-liml"))
	    ci = IV_LIML;
	else if (!strcmp(s, "iv-gmm"))
	    ci = IV_GMM;
	else if (!strcmp(s, "countmod"))
	    ci = COUNTMOD;
    }

    return ci;
}

void fit_resid_callback (GtkAction *action, gpointer data)
{
    const gchar *s = gtk_action_get_name(action);
    windata_t *vwin = (windata_t *) data; 
    int code = M_UHAT; 

    if (!strcmp(s, "yhat")) {
	code = M_YHAT;
    } else if (!strcmp(s, "uhat")) {
	code = M_UHAT;
    } else if (!strcmp(s, "uhat2")) {
	code = M_UHAT2;
    } else if (!strcmp(s, "h")) {
	code = M_H;
    } else if (!strcmp(s, "ahat")) {
	code = M_AHAT;
    }

    save_fit_resid(vwin, code);
}

/* callback for adding a scalar from a model */

void model_stat_callback (GtkAction *action, gpointer data)
{
    const gchar *s = gtk_action_get_name(action);
    windata_t *vwin = (windata_t *) data; 
    MODEL *pmod = vwin->data;
    int code = M_ESS; 

    if (!strcmp(s, "ess")) {
	code = M_ESS;
    } else if (!strcmp(s, "se")) {
	code = M_SIGMA;
    } else if (!strcmp(s, "rsq")) {
	code = M_RSQ;
    } else if (!strcmp(s, "trsq")) {
	code = M_TRSQ;
    } else if (!strcmp(s, "df")) {
	code = M_DF;
    } else if (!strcmp(s, "lnL")) {
	code = M_LNL;
    } else if (!strcmp(s, "AIC")) {
	code = M_AIC;
    } else if (!strcmp(s, "BIC")) {
	code = M_BIC;
    } else if (!strcmp(s, "HQC")) {
	code = M_HQC;
    }

    add_model_stat(pmod, code, vwin);
}

void model_callback (GtkAction *action, gpointer data) 
{
    int code = model_action_code(action);

    modelspec_dialog(code);
}

void model_genr_callback (GtkAction *action, gpointer data)
{
    windata_t *vwin = (windata_t *) data;

    edit_dialog(MODEL_GENR, _("gretl: add var"), 
		_("Enter formula for new variable:"),
		"", do_model_genr, vwin, 
		VARCLICK_INSERT_NAME, 
		vwin_toplevel(vwin));   
}

static int selector_callback_code (const gchar *s)
{
    int ci = gretl_command_number(s);

    if (ci > 0) return ci;

    if (!strcmp(s, "TSPlot"))
	return GR_PLOT;
    if (!strcmp(s, "ScatterPlot"))
	return GR_XY;
    if (!strcmp(s, "ImpulsePlot"))
	return GR_IMP;
    if (!strcmp(s, "FactorPlot"))
	return GR_DUMMY;
    if (!strcmp(s, "FrischPlot"))
	return GR_XYZ;
    if (!strcmp(s, "ThreeDPlot"))
	return GR_3D;
    if (!strcmp(s, "MultiXY"))
	return SCATTERS;
    if (!strcmp(s, "MultiTS"))
	return TSPLOTS;
    if (!strcmp(s, "GR_FBOX"))
	return GR_FBOX;
    if (!strcmp(s, "VLAGSEL"))
	return VLAGSEL;
    if (!strcmp(s, "ConfEllipse"))
	return ELLIPSE;
    if (!strcmp(s, "VarOmit"))
	return VAROMIT;
    if (!strcmp(s, "loess"))
	return LOESS;
    if (!strcmp(s, "nadarwat"))
	return NADARWAT;

    return 0;
}

void selector_callback (GtkAction *action, gpointer data)
{
    const gchar *s = gtk_action_get_name(action);
    windata_t *vwin = (windata_t *) data;
    int ci;

    ci = selector_callback_code(s);

    if (ci == COINT || ci == COINT2) {
	selection_dialog(ci, _("gretl: cointegration test"), do_coint);
    } else if (ci == VAR || ci == VECM) {
	selection_dialog(ci, (ci == VAR)? _("gretl: VAR") : _("gretl: VECM"),
			 do_vector_model);
    } else if (ci == VLAGSEL) {
	selection_dialog(ci, _("gretl: VAR lag selection"), do_vector_model);
    } else if (ci == GR_XY || ci == GR_IMP || ci == GR_DUMMY ||
	       ci == SCATTERS || ci == GR_3D || ci == GR_XYZ ||
	       ci == GR_FBOX) {
	int (*selfunc)() = NULL;

	switch (ci) {
	case GR_XY:
	case GR_IMP:
	    selfunc = do_graph_from_selector;
	    break;
	case GR_3D:
	    selfunc = do_splot_from_selector;
	    break;
	case GR_DUMMY:
	    selfunc = do_dummy_graph;
	    break;
	case GR_XYZ:
	    selfunc = do_xyz_graph;
	    break;
	case SCATTERS:
	    selfunc = do_scatters;
	    break;
	case GR_FBOX:
	    selfunc = do_factorized_boxplot;
	    break;
	default:
	    return;
	}
	selection_dialog(ci, _("gretl: define graph"), selfunc);
    } else if (ci == ADD || ci == OMIT) {
	simple_selection_for_viewer(ci, _("gretl: model tests"), 
				    do_add_omit, vwin);
    } else if (ci == VAROMIT) {
	simple_selection_for_viewer(ci, _("gretl: model tests"), 
				    do_VAR_omit, vwin);
    } else if (ci == COEFFSUM) {
	simple_selection_for_viewer(ci, _("gretl: model tests"), 
				    do_coeff_sum, vwin);
    } else if (ci == ELLIPSE) {
	simple_selection_for_viewer(ci, _("gretl: model tests"), 
				    do_confidence_region, vwin);
    } else if (ci == GR_PLOT) {
	simple_selection(ci, _("gretl: define graph"), do_graph_from_selector, 
			 NULL);
    } else if (ci == TSPLOTS) {
	simple_selection(ci, _("gretl: define graph"), do_scatters, NULL);
    } else if (ci == SPEARMAN) {
	char title[64];
	
	strcpy(title, "gretl: ");
	strcat(title, _("rank correlation"));
	simple_selection(ci, title, do_rankcorr, NULL);
    } else if (ci == LOESS || ci == NADARWAT) {
	char title[64];
	
	strcpy(title, "gretl: ");
	strcat(title, (ci == LOESS)? _("Loess") : _("Nadaraya-Watson"));
	selection_dialog(ci, title, do_nonparam_model);
    } else {
	errbox("selector_callback: code was not recognized");
    }
}

static int gretl_callback_code (const gchar *s)
{
    if (!strcmp(s, "SampleRestrict")) 
	return SMPLBOOL;
    if (!strcmp(s, "GENR")) 
	return GENR;
    if (!strcmp(s, "VSETMISS")) 
	return VSETMISS;
    if (!strcmp(s, "GSETMISS")) 
	return GSETMISS;
    if (!strcmp(s, "GR_BOX")) 
	return GR_BOX;
    if (!strcmp(s, "gmm")) 
	return GMM;
    if (!strcmp(s, "mle")) 
	return MLE;
    if (!strcmp(s, "nls")) 
	return NLS;
    if (!strcmp(s, "system")) 
	return SYSTEM;
    if (!strcmp(s, "restrict")) 
	return RESTRICT;
    if (!strcmp(s, "MINIBUF")) 
	return MINIBUF;
    return 0;
}

/* callback for menu items, where we want to prepare an "edit dialog"
   to handle the request */

void gretl_callback (GtkAction *action, gpointer data)
{
    windata_t *vwin = (windata_t *) data;
    GtkWidget *parent = NULL;
    const char *title = NULL;
    const char *query = NULL;
    const char *defstr = NULL;
    gchar *dynquery = NULL;
    void (*okfunc)() = NULL;
    Varclick click = VARCLICK_NONE;
    int ci;

    ci = gretl_callback_code(gtk_action_get_name(action));

    switch (ci) {
    case GENR:
	title = N_("gretl: add var");
	if (dataset->n > 5000) {
	    query = N_("Enter formula for new variable");
	} else {
	    query = N_("Enter formula for new variable\n"
		       "(or just a name, to enter data manually)");
	}
	okfunc = do_genr;
	click = VARCLICK_INSERT_NAME;
	break;
    case VSETMISS:
	title = N_("gretl: missing code");
	dynquery = g_strdup_printf(_("Enter value to be read as \"missing\"\n"
				     "for the variable \"%s\""), 
				   dataset->varname[mdata->active_var]);
	okfunc = do_variable_setmiss;
	break;
    case GSETMISS:
	title = N_("gretl: missing code");
	query = N_("Enter value to be read as \"missing\":");
	okfunc = do_global_setmiss;
	break;
    case GR_BOX:
	title = N_("gretl: boxplots");
	query = N_("Specify variables to plot:");
	okfunc = do_box_graph;
	click = VARCLICK_INSERT_NAME;
	defstr = get_last_boxplots_string();
	break;
    case GMM:
	title = N_("gretl: GMM");
	query = N_("GMM: Specify function and orthogonality conditions:");
	okfunc = do_gmm_model;
	click = VARCLICK_INSERT_TEXT;
	data = NULL;
	break;	
    case MLE:
	title = N_("gretl: maximum likelihood");
	query = N_("MLE: Specify function, and derivatives if possible:");
	okfunc = do_mle_model;
	click = VARCLICK_INSERT_TEXT;
	data = NULL;
	break;	
    case NLS:
	title = N_("gretl: nonlinear least squares");
	query = N_("NLS: Specify function, and derivatives if possible:");
	okfunc = do_nls_model;
	click = VARCLICK_INSERT_TEXT;
	data = NULL;
	break;	
    case SYSTEM:
	title = N_("gretl: simultaneous equations system");
	query = N_("Specify simultaneous equations:");
	data = NULL;
	okfunc = do_eqn_system;
	click = VARCLICK_INSERT_TEXT;
	break;
    case RESTRICT:
	title = N_("gretl: linear restrictions");
	query = N_("Specify restrictions:");
	okfunc = do_restrict;
	parent = vwin_toplevel(vwin);
	break;	
    case MINIBUF:
	title = N_("gretl: command entry");
	query = N_("Type a command:");
	okfunc = do_minibuf;
	break;	
    default:
	fprintf(stderr, "gretl_callback: unrecognized action '%s'\n", 
	       gtk_action_get_name(action));
	return;
    }

    if (dynquery != NULL) {
	edit_dialog(ci, _(title), dynquery, defstr, okfunc, data, 
		    click, parent);
	g_free(dynquery);
    } else {
	edit_dialog(ci, _(title), _(query), defstr, okfunc, data, 
		    click, parent);
    }
}

void menu_boxplot_callback (int varnum)
{
    const char *opts[] = {
	N_("Simple boxplot"),
	N_("With confidence interval for median"),
	N_("Factorized")
    };
    int ret;

    ret = radio_dialog(_("gretl: define graph"), NULL, 
		       opts, 3, 0, 0, NULL);

    if (ret == 0) {
	do_boxplot_var(varnum, OPT_NONE);
    } else if (ret == 1) {
	do_boxplot_var(varnum, OPT_O);
    } else if (ret == 2) {
	selector_set_varnum(varnum);
	selection_dialog(GR_FBOX, _("gretl: define graph"), 
			 do_factorized_boxplot);
    }
}

void boxplot_callback (void)
{
    menu_boxplot_callback(mdata_active_var());
}

void revise_nl_model (MODEL *pmod, GtkWidget *parent)
{
    const char *title = NULL;
    const char *query = NULL;
    const char *defstr;
    void (*okfunc)() = NULL;

    switch (pmod->ci) {
    case GMM:
	title = N_("gretl: GMM");
	query = N_("GMM: Specify function and orthogonality conditions:");
	okfunc = do_gmm_model;
	break;	
    case MLE:
	title = N_("gretl: maximum likelihood");
	query = N_("MLE: Specify function, and derivatives if possible:");
	okfunc = do_mle_model;
	break;	
    case NLS:
	title = N_("gretl: nonlinear least squares");
	query = N_("NLS: Specify function, and derivatives if possible:");
	okfunc = do_nls_model;
	break;	
    }

    defstr = gretl_model_get_data(pmod, "nlinfo");

    edit_dialog(pmod->ci, _(title), _(query), defstr, okfunc, pmod, 
		VARCLICK_INSERT_TEXT, parent);   
}

void revise_system_model (void *ptr, GtkWidget *parent)
{
    equation_system *sys = (equation_system *) ptr;

    edit_dialog(SYSTEM, _("gretl: simultaneous equations system"), 
		_("Specify simultaneous equations:"), NULL,
		do_eqn_system, sys,
		VARCLICK_INSERT_TEXT,
		parent);
}

void genr_callback (void)
{
    const char *msg;

    if (dataset->n > 5000) {
	msg = N_("Enter formula for new variable");
    } else {
	msg = N_("Enter formula for new variable\n"
		 "(or just a name, to enter data manually)");
    }

    edit_dialog(GENR, _("gretl: add var"), _(msg),
		NULL, do_genr, NULL, 
		VARCLICK_INSERT_NAME, NULL);   
}

void minibuf_callback (void)
{
    edit_dialog(MINIBUF, _("gretl: command entry"),
		_("Type a command:"), NULL, 
		do_minibuf, NULL, 
		VARCLICK_NONE, NULL);   
}

void newdata_callback (void) 
{
    int resp, n = 50;

    if (dataset_locked()) {
	return;
    }

    resp = spin_dialog(_("gretl: create data set"), NULL, &n, 
		       _("Number of observations:"), 
		       2, 1000000, 0, NULL);

    if (resp < 0) {
	/* canceled */
	return;
    }

    if (open_nulldata(dataset, data_status, n, OPT_NONE, NULL)) {
	errbox(_("Failed to create empty data set"));
	return;
    }

    new_data_structure_dialog();
}

void xcorrgm_callback (void)
{
    if (mdata_selection_count() == 2) {
	do_xcorrgm(NULL);
    } else {
	char title[64];

	strcpy(title, "gretl: ");
	strcat(title, _("cross-correlogram"));
	simple_selection(XCORRGM, title, do_xcorrgm, NULL);
    }
}

static int nist_verbosity (GtkAction *action)
{
    const gchar *s = gtk_action_get_name(action);

    if (!strcmp(s, "NistVVerbose")) 
	return 2;
    else if (!strcmp(s, "NistVerbose")) 
	return 1;
    else 
	return 0;
}

void do_nistcheck (GtkAction *action)
{
    void *handle;
    int (*run_nist_tests)(const char *, const char *, int);
    gchar *datadir = NULL;
    gchar *fname = NULL;
    
    run_nist_tests = gui_get_plugin_function("run_nist_tests", 
					     &handle);
    if (run_nist_tests == NULL) {
	return;
    }

    datadir = g_strdup_printf("%sdata%s", gretl_home(), SLASHSTR);
    fname = g_strdup_printf("%snist.out", gretl_dotdir());

    (*run_nist_tests)(datadir, fname, nist_verbosity(action));

    close_plugin(handle);

    view_file(fname, 0, 1, 78, 400, VIEW_CODEBOOK);

    g_free(datadir);
    g_free(fname);
}

#if defined(ENABLE_MAILER) && !defined(G_OS_WIN32)

void send_file (char *fullname)
{
    int (*email_file) (const char *, const char *);
    void *handle;

    email_file = gui_get_plugin_function("email_file", &handle);
    if (email_file == NULL) {
        return;
    }
    
    email_file(fullname, gretl_dotdir());
    close_plugin(handle);
}

#endif
