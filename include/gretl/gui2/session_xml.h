
/* addendum to session.c, for handling the saving of session info to
   an XML file, and the re-building of a session from same.
*/

#ifndef SESSION_XML_H
#define SESSION_XML_H

#define FULL_XML_HEADERS

#include <gretl/gui2/gretl.h>
#include <gretl/gui2/session.h>
#include <gretl/gui2/selector.h>
#include <gretl/gui2/ssheet.h>
#include <gretl/lib/plotspec.h>
#include <gretl/gui2/gpt_control.h>
#include <gretl/gui2/guiprint.h>
#include <gretl/gui2/model_table.h>
#include <gretl/gui2/graph_page.h>
#include <gretl/gui2/textbuf.h>
#include <gretl/gui2/cmdstack.h>
#include <gretl/gui2/filelists.h>
#include <gretl/gui2/dlgutils.h>
#include <gretl/gui2/fileselect.h>
#include <gretl/gui2/menustate.h>
#include <gretl/gui2/toolbar.h>
#include <gretl/gui2/winstack.h>
#include <gretl/gui2/fncall.h>
#include <gretl/gui2/lib_private.h>

#include <gretl/lib/var.h>
#include <gretl/lib/varprint.h>
#include <gretl/lib/objstack.h>
#include <gretl/lib/system.h>
#include <gretl/lib/gretl_xml.h>
#include <gretl/lib/gretl_func.h>
#include <gretl/lib/matrix_extra.h>
#include <gretl/lib/cmd_private.h>
#include <gretl/lib/uservar.h>
#include <gretl/lib/gretl_zip.h>
#include <gretl/lib/usermat.h>
#include <gretl/lib/boxplots.h>
#include <gretl/lib/libset.h>

typedef struct SESSION_ SESSION;
typedef struct SESSION_TEXT_ SESSION_TEXT;
typedef struct SESSION_MODEL_ SESSION_MODEL;
typedef struct SESSION_GRAPH_ SESSION_GRAPH;
typedef struct gui_obj_ gui_obj;

enum {
    SESSION_CHANGED    = 1 << 0,
    SESSION_SAVED      = 1 << 1,
    SESSION_OPEN       = 1 << 2
};

struct SESSION_ {
    char name[MAXLEN];
    char dirname[MAXLEN];
    int status;
    int show_notes;
    int nmodels;
    int ngraphs;
    int ntexts;
    SESSION_GRAPH **graphs;
    SESSION_MODEL **models;
    SESSION_TEXT **texts;
    char *notes;
};

struct SESSION_TEXT_ {
    char name[MAXSAVENAME];
    char *buf;
};

struct SESSION_MODEL_ {
    char name[MAXSAVENAME];
    void *ptr;
    GretlObjType type;
};

struct SESSION_GRAPH_ {
    char name[MAXSAVENAME];
    char fname[MAXLEN];
    GretlObjType type;
};

struct gui_obj_ {
    gchar *name;
    gint sort;
    gpointer data;
    GtkWidget *icon;
    GtkWidget *label;
    gint row, col;
};

struct sample_info {
    char datafile[MAXLEN];
    int t1;
    int t2;
    char *mask;
    char *restriction;
    unsigned int seed;
    int resample_n;
};

enum {
    ICON_ADD_BATCH,
    ICON_ADD_SINGLE
} icon_add_modes;

static char *global_items[] = {
    N_("Save session"),
    N_("Arrange icons"),
    N_("Windows"),
    N_("Close window")
};

static char *model_items[] = {
    N_("Display"),
    N_("Add to model table"),
    N_("Rename"),
    N_("Delete")
};

static char *model_table_items[] = {
    N_("Display"),
    N_("Clear"),
    N_("Help")
};

static char *graph_page_items[] = {
    N_("Display"),
    N_("Save as TeX..."),
    N_("Clear"),
    N_("Help")
};

static char *generic_items[] = {
    N_("Display"),
    N_("Rename"),
    N_("Delete")
};

static char *graph_items[] = {
    N_("Display"),
    N_("Edit plot commands"),
    N_("Rename"),
    N_("Delete"),
    N_("Copy")
};

static char *dataset_items[] = {
    N_("Edit"),
    N_("Export as CSV..."),
    N_("Copy as CSV...")
};

static char *scalars_items[] = {
    N_("Edit"),
    N_("Copy as CSV...")
};

static char *info_items[] = {
    N_("View")
};

static char *matrix_items[] = {
    N_("View"),
    N_("Edit"),
    N_("Properties"),
    N_("Copy as CSV..."),
    N_("Rename"),
    N_("Delete"),
    N_("Copy")
};

static char *bundle_items[] = {
    N_("View"),
    N_("Rename"),
    N_("Delete")
};

/* file-scope globals */

SESSION session;            /* hold models, graphs */

static char sessionfile[MAXLEN];

static GtkWidget *iconview;
static GtkWidget *icon_table;
static GtkWidget *global_popup;
static GtkWidget *model_popup;
static GtkWidget *model_table_popup;
static GtkWidget *generic_popup;
static GtkWidget *graph_popup;
static GtkWidget *graph_page_popup;
static GtkWidget *data_popup;
static GtkWidget *scalars_popup;
static GtkWidget *info_popup;
static GtkWidget *matrix_popup;
static GtkWidget *bundle_popup;
static GtkWidget *save_item;

static GList *iconlist;
static gui_obj *active_object;
static gint iconview_width;
static gint iconview_cols;
static gint in_icon;

/* private functions */
static gui_obj *gui_object_new (gchar *name, int sort, gpointer data);
static gui_obj *session_add_icon (gpointer data, int sort, int mode);
static void session_build_popups (void);
static void global_popup_callback (GtkWidget *widget, gpointer data);
static void object_popup_callback (GtkWidget *widget, gpointer data);
static void data_popup_callback (GtkWidget *widget, gpointer data);
static void scalars_popup_callback (GtkWidget *widget, gpointer data);
static void info_popup_callback (GtkWidget *widget, gpointer data);
static void matrix_popup_callback (GtkWidget *widget, gpointer data);
static void bundle_popup_callback (GtkWidget *widget, gpointer data);
static void session_delete_icon (gui_obj *obj);
static void open_gui_graph (gui_obj *obj);
static gboolean session_view_click (GtkWidget *widget,
                    GdkEventButton *event,
                    gpointer data);
static int real_delete_model_from_session (SESSION_MODEL *model);
static void make_short_label_string (char *targ, const char *src);
static gui_obj *get_gui_obj_by_data (void *finddata);
static int gui_user_var_callback (const char *name, GretlType type,
                  int action);
static void auto_view_session (void);

static int session_graph_count;
static int session_bundle_count;
static int commands_recorded;

int check_graph_file (const char *fname, int type);

/* Arrange things so that when a session file is opened, any graph
   files are numbered consecutively, starting at 1.  This avoids a
   situation where adding a graph to an existing session results in
   over-writing an existing graph file.  (The graph files in a saved
   session may not be numbered consecutively if some graphs were
   initially saved, then deleted; and it's easier to fix this on
   opening a session file than on saving one, since on opening we're
   constructing the graph list de novo.)
*/

void normalize_graph_filename (char *fname, int gnum);

int restore_session_graphs (xmlNodePtr node);

int restore_session_texts (xmlNodePtr node, xmlDocPtr doc);

int data_submask_from_xml (xmlNodePtr node, xmlDocPtr doc,
                  struct sample_info *sinfo);

int data_restrict_from_xml (xmlNodePtr node, xmlDocPtr doc,
                   struct sample_info *sinfo);

int rebuild_session_model (const char *fname,
                  const char *name,
                  GretlObjType type,
                  int tablepos);

int restore_session_models (xmlNodePtr node, xmlDocPtr doc);

/* peek inside the session file and retrieve the name of the
   data file, only */

int get_session_datafile_name (const char *fname, struct sample_info *sinfo,
                      int *nodata);

/* (having previously grabbed the data file name) get the rest
   of the info from session.xml */

int
read_session_xml (const char *fname, struct sample_info *sinfo);

int maybe_read_functions_file (const char *fname);

int maybe_read_settings_file (const char *fname);

int model_in_session (const void *ptr);

SavedObjectFlags model_save_flags (const void *ptr,
                      GretlObjType type);

int maybe_write_function_file (char *fullname);

int write_settings_file (char *fullname);

char *get_xmlname (char *objname, int *err);

int write_session_xml (const char *datname);

#endif // SESSION_XML_H
