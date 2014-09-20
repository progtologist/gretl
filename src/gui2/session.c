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

/* session.c for gretl */

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
#include <gretl/gui2/session_xml.h>

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#ifdef _WIN32
# include <windows.h>
# include "gretlwin32.h"
#endif

#include <gretl/pixmaps/model.xpm>
#include <gretl/pixmaps/boxplot.xpm>
#include <gretl/pixmaps/gnuplot.xpm>
#include <gretl/pixmaps/xfm_sc.xpm>
#include <gretl/pixmaps/xfm_info.xpm>
#include <gretl/pixmaps/xfm_text.xpm>
#include <gretl/pixmaps/rhohat.xpm>
#include <gretl/pixmaps/summary.xpm>
#include <gretl/pixmaps/model_table.xpm>
#include <gretl/pixmaps/graph_page.xpm>
#include <gretl/pixmaps/matrix.xpm>
#include <gretl/pixmaps/bundle.xpm>

#define SESSION_DEBUG 0

#define SHOWNAMELEN 15
#define ICONVIEW_MIN_COLS 4

int session_is_modified (void)
{
    return session.status & SESSION_CHANGED;
}

int session_is_open (void)
{
    return session.status & SESSION_OPEN;
}

void mark_session_changed (void)
{
    iconview_menubar_state(TRUE);
    session.status &= ~SESSION_SAVED;
    session.status |= SESSION_CHANGED;
    if (save_item != NULL) {
	gtk_widget_set_sensitive(save_item, TRUE);
    }
    flip(mdata->ui, "/menubar/File/SessionFiles/SaveSession", TRUE);
    flip(mdata->ui, "/menubar/File/SessionFiles/SaveSessionAs", TRUE);
    if (*session.name != '\0') {
	set_main_window_title(session.name, TRUE);
    }
}

static void mark_session_saved (void)
{
    session.status &= ~SESSION_CHANGED;
    session.status |= SESSION_SAVED;
    if (save_item != NULL) {
	gtk_widget_set_sensitive(save_item, FALSE);
    }
    flip(mdata->ui, "/menubar/File/SessionFiles/SaveSession", FALSE);
    flip(mdata->ui, "/menubar/File/SessionFiles/SaveSessionAs", 
	 session.status & SESSION_OPEN);
    set_main_window_title(session.name, FALSE);
}

void set_commands_recorded (void)
{
    commands_recorded = 1;
}

int get_commands_recorded (void)
{
    return commands_recorded;
}

int widget_is_iconview (GtkWidget *w)
{
    return w == iconview;
}

/* constructors and destructors for session data-objects */

static void free_session_text (SESSION_TEXT *text)
{
    free(text->buf);
    free(text);
}

static void free_session_model (SESSION_MODEL *mod)
{
#if SESSION_DEBUG
    fprintf(stderr, "free_session_model: ptr at %p\n", (void *) mod->ptr);
#endif
    gretl_object_remove_from_stack(mod->ptr, mod->type);
    free(mod);
}

SESSION_MODEL *session_model_new (void *ptr, const char *name,
					 GretlObjType type)
{
    SESSION_MODEL *mod = mymalloc(sizeof *mod);

    if (mod != NULL) {
	mod->ptr = ptr;
	mod->type = type;
	gretl_stack_object_as(ptr, type, name);
	strcpy(mod->name, gretl_object_get_name(ptr, type));
	/* note: we don't add a reference here: that's handled by the
	   mechanism in objstack.c 
	*/
    }

    return mod;
}

int session_append_text (const char *tname, char *buf)
{
    SESSION_TEXT *text;
    SESSION_TEXT **texts;
    int nt = session.ntexts;

    text = mymalloc(sizeof *text);
    if (text == NULL) {
	return 1;
    }

    text->buf = buf;

    *text->name = '\0';
    if (tname != NULL) {
	strncat(text->name, tname, MAXSAVENAME - 1);
    }

    texts = myrealloc(session.texts, (nt + 1) * sizeof *texts);

    if (texts == NULL) {
	free_session_text(text);
	return 1;
    }

    session.texts = texts;
    session.texts[nt] = text;
    session.ntexts += 1;

    return 0;
}

int session_append_model (SESSION_MODEL *mod)
{
    SESSION_MODEL **models;
    int nm = session.nmodels;

    models = myrealloc(session.models, (nm + 1) * sizeof *models);
    if (models == NULL) {
	free_session_model(mod);
	return 1;
    }

    session.models = models;
    session.models[nm] = mod;
    session.nmodels += 1;

#if SESSION_DEBUG
    fprintf(stderr, "session_append_model: nmodels now = %d\n",
	    session.nmodels);
#endif

    return 0;
}

SESSION_GRAPH *session_append_graph (const char *grname,
					    const char *fname,
					    GretlObjType type)
{
    SESSION_GRAPH *graph;
    SESSION_GRAPH **graphs;
    int ng = session.ngraphs;

    graph = mymalloc(sizeof *graph);
    if (graph == NULL) {
	return NULL;
    }

    *graph->name = '\0';
    if (grname != NULL) {
	strncat(graph->name, grname, MAXSAVENAME - 1);
    }

    *graph->fname = '\0';
    if (fname != NULL) {
	strncat(graph->fname, fname, MAXLEN - 1);
    }	

    graph->type = type;

    graphs = myrealloc(session.graphs, (ng + 1) * sizeof *graphs);

    if (graphs == NULL) {
	free(graph);
	return NULL;
    }

    session.graphs = graphs;
    session.graphs[ng] = graph;
    session.ngraphs++;     /* decremented if graph is deleted */
    session_graph_count++; /* not decremented on deletion */

    return graph;
}

const char *last_session_graph_name (void)
{
    int ng = session.ngraphs;

    if (ng > 0) {
	SESSION_GRAPH *graph = session.graphs[ng - 1];

	return graph->fname;
    } else {
	return NULL;
    }
}

static void session_switch_log_location (int code)
{
    gchar *fullpath;

    fullpath = g_strdup_printf("%s%s%c", gretl_dotdir(), 
			       session.dirname, SLASH);
    set_session_log(fullpath, code);
    g_free(fullpath);
}

char *session_graph_make_path (char *path, const char *fname)
{
    if (strstr(fname, session.dirname) != NULL) {
	/* should be OK */
	strcpy(path, fname);
    } else {
	char *p = strrchr(fname, SLASH);

	if (p != NULL) {
	    sprintf(path, "%s%s", session.dirname, p);
	} else {
	    sprintf(path, "%s%c%s", session.dirname, SLASH, fname);
	}
    }

    return path;
}

/* first arg should be a MAXLEN string */

char *session_file_make_path (char *path, const char *fname)
{
#if SESSION_DEBUG
    fprintf(stderr, "session_file_make_path: fname = '%s'\n", fname);
#endif

    if (g_path_is_absolute(fname)) {
	strcpy(path, fname);
    } else {
	sprintf(path, "%s%c%s", session.dirname, SLASH, fname);
    } 

#if SESSION_DEBUG
    fprintf(stderr, "session_file_make_path: path = '%s'\n", path);
#endif

    return path;
}

static void edit_session_notes (void)
{
    static GtkWidget *notes_window;

    if (notes_window == NULL) {
	windata_t *vwin;

	vwin = edit_buffer(&session.notes, 80, 400, 
			   _("gretl: session notes"),
			   EDIT_NOTES);
	notes_window = vwin->main;
	g_signal_connect(G_OBJECT(vwin->main), "destroy",
			 G_CALLBACK(gtk_widget_destroyed),
			 &notes_window);
    } else {
	gtk_window_present(GTK_WINDOW(notes_window));
    }
}

int is_session_model (void *p)
{
    int i;

#if SESSION_DEBUG
    fprintf(stderr, "is_session_model: testing %p (nmodels = %d)\n", 
	    p, session.nmodels);
#endif

    for (i=0; i<session.nmodels; i++) {
	if (p == session.models[i]->ptr) {
	    return 1;
	}
    }
    return 0;
}

static SESSION_MODEL *get_session_model_by_name (const char *name)
{
    int i;

    for (i=0; i<session.nmodels; i++) {
	if (!strcmp(name, session.models[i]->name)) {
	    return session.models[i];
	}
    }

    return NULL;
}

static SESSION_GRAPH *get_session_graph_by_name (const char *name)
{
    int i;

    for (i=0; i<session.ngraphs; i++) {
	if (!strcmp(name, session.graphs[i]->name)) {
	    return session.graphs[i];
	}
    }

    return NULL;
}

static SESSION_TEXT *get_session_text_by_name (const char *name)
{
    int i;

    for (i=0; i<session.ntexts; i++) {
	if (!strcmp(name, session.texts[i]->name)) {
	    return session.texts[i];
	}
    }

    return NULL;
}

int real_add_text_to_session (PRN *prn, const char *tname)
{
    SESSION_TEXT *text = get_session_text_by_name(tname);
    char *buf = gretl_print_get_chunk(prn);
    int replace = 0;

    if (buf == NULL) {
	return ADD_OBJECT_FAIL;
    }

    if (text != NULL) {
	free(text->buf);
	text->buf = buf;
	replace = 1;
    } else {
	if (session_append_text(tname, buf)) {
	    return ADD_OBJECT_FAIL;
	}
    }
    
    mark_session_changed();

    if (iconlist != NULL && !replace) {
	session_add_icon(text, GRETL_OBJ_TEXT, ICON_ADD_SINGLE);
    }

    return (replace)? ADD_OBJECT_REPLACE : ADD_OBJECT_OK;
}

static int ends_with_number (const char *s, int *k)
{
    int ret = 0;

    s = strrchr(s, '(');

    if (s != NULL) {
	int n = strspn(s + 1, "0123456789");
	
	if (n > 0 && *(s + n + 1) == ')' && *(s + n + 2) == '\0') {
	    sscanf(s + 1, "%d", k);
	    ret = 1;
	}
    }

    return ret;
}

static void ensure_unique_text_name (char *tname)
{
    char tmp[MAXSAVENAME];
    const char *p, *oldname;
    int i, n, id, idmax = 0;

    for (i=0; i<session.ntexts; i++) {
	id = 0;
	oldname = session.texts[i]->name;
	if (!strcmp(tname, oldname)) {
	    id = 1;
	} else if (ends_with_number(oldname, &id)) {
	    p = strrchr(oldname, '(');
	    *tmp = '\0';
	    strncat(tmp, oldname, p - oldname);
	    if (strcmp(tmp, tname)) {
		id = 0;
	    }
	}
	if (id > idmax) {
	    idmax = id;
	}	
    }

    if (idmax > 0) {
	char num[8];

	sprintf(num, "(%d)", ++idmax);
	n = MAXSAVENAME - strlen(num) - 1;
	*tmp = '\0';
	strncat(tmp, tname, n);
	strcat(tmp, num);
	strcpy(tname, tmp);
    }
}

void save_output_as_text_icon (windata_t *vwin)
{
    const gchar *title = NULL;
    char tname[MAXSAVENAME];
    gchar *buf;
    int err;

    buf = textview_get_text(vwin->text);
    if (buf == NULL) {
	errbox("Couldn't retrieve buffer");
	return;
    }

    if (vwin->main != NULL) {
	title = gtk_window_get_title(GTK_WINDOW(vwin->main));
    }

    if (title != NULL && !strncmp(title, "gretl: ", 7)) {
	int n = strlen(title + 7);

	if (n > MAXSAVENAME - 1) {
	    n = MAXSAVENAME - 1;
	}
	*tname = '\0';
	strncat(tname, title + 7, n);
    } else {
	strcpy(tname, "text");
    }

    ensure_unique_text_name(tname);
    err = session_append_text(tname, buf);

    if (err) {
	return;
    } else if (iconlist != NULL) {
	SESSION_TEXT * text = get_session_text_by_name(tname);

	session_add_icon(text, GRETL_OBJ_TEXT, ICON_ADD_SINGLE);
    } else if (autoicon_on()) {
	auto_view_session();
    }
}

static int add_model_to_session (void *ptr, const char *name,
				 GretlObjType type)
{
    SESSION_MODEL *mod;
    int err = 0;

#if SESSION_DEBUG
    fprintf(stderr, "add_model_to_session: doing session_model_new\n"
	    " with ptr = %p\n", ptr);
#endif

    mod = session_model_new(ptr, name, type);

    if (mod == NULL || session_append_model(mod)) {
	err = E_ALLOC;
    } else if (iconlist != NULL) {
	session_add_icon(mod, type, ICON_ADD_SINGLE);
    } else if (autoicon_on()) {
	auto_view_session();
    }

    return err;
}

static int replace_session_graph (SESSION_GRAPH *graph,
				  const char *fname,
				  GretlObjType type)
{
    if (fname != NULL && strcmp(graph->fname, fname)) {
	gretl_remove(graph->fname);
	strcpy(graph->fname, fname);
    }

    graph->type = type;
    mark_session_changed();

    return ADD_OBJECT_REPLACE;
}

static int 
real_add_graph_to_session (const char *fname, const char *grname,
			   GretlObjType type)
{
    SESSION_GRAPH *graph = get_session_graph_by_name(grname);

    if (graph != NULL) {
	return replace_session_graph(graph, grname, type);
    } 

    graph = session_append_graph(grname, fname, type);
    if (graph == NULL) {
	return ADD_OBJECT_FAIL;
    }

    mark_session_changed();

    if (iconlist != NULL) {
	session_add_icon(graph, type, ICON_ADD_SINGLE);
    } else if (autoicon_on()) {
	auto_view_session();
    }

    return ADD_OBJECT_OK;
}

const char *get_session_dirname (void)
{
    return session.dirname;
}

static int session_dir_ok (void)
{
    int ret = 1;

    if (*session.dirname == '\0') {
	pid_t p = getpid();

	errno = 0;

	sprintf(session.dirname, ".gretl-%d", (int) p);
	if (gretl_chdir(gretl_dotdir())) {
	    perror("moving to user directory");
	    ret = 0;
	} else if (gretl_mkdir(session.dirname)) {
	    ret = 0;
	}
    }

    return ret;
}

static void make_graph_name (char *shortname, char *graphname) 
{
    int i, n = session_graph_count + 1;

    for ( ; ; n++) {
	int unique = 1;

	sprintf(shortname, "graph.%d", n);
	sprintf(graphname, "%s %d", _("Graph"), n);

	for (i=0; i<session.ngraphs; i++) {
	    if (!strcmp(shortname, session.graphs[i]->fname) ||
		!strcmp(graphname, session.graphs[i]->name)) {
		unique = 0;
		break;
	    }
	}

	if (unique) {
	    break;
	} 
    }
}

/* Callback for "add to session as icon" on a graph displayed
   as PNG -- see gpt_control.c. Note that there is code in
   gpt_control designed to ensure that this option is not
   available for a graph that has already been saved in
   this way.
*/

int gui_add_graph_to_session (char *fname, char *fullname, int type)
{
    char shortname[MAXSAVENAME];
    char graphname[MAXSAVENAME];
    int err = 0;

    if (type != GRETL_OBJ_PLOT && (dataset == NULL || dataset->Z == NULL)) {
	/* we may be called via the "stats calculator" when
	   there's no dataset yet */
	err = open_nulldata(dataset, 0, 10, OPT_NONE, NULL);
	if (err) {
	    gui_errmsg(err);
	    return 1;
	}
	register_data(NULLDATA_STARTED);
    }
    
    errno = 0;

    if (!session_dir_ok()) {
	errbox(_("Failed to copy graph file"));
	return 1;
    }

    gretl_chdir(gretl_dotdir());
    make_graph_name(shortname, graphname);
    session_file_make_path(fullname, shortname);

    /* move temporary plot file to permanent */
    if (copyfile(fname, fullname)) {
	return 1;
    } 

    gretl_remove(fname);
    strcpy(fname, fullname); /* was @shortname */

    err = real_add_graph_to_session(shortname, graphname, type);
    if (err == ADD_OBJECT_FAIL) {
	err = 1;
    }

    return err;
}

static void make_graph_filename (char *shortname) 
{
    int i, n = session_graph_count + 1;

    for ( ; ; n++) {
	int unique = 1;

	sprintf(shortname, "graph.%d", n);

	for (i=0; i<session.ngraphs; i++) {
	    if (!strcmp(shortname, session.graphs[i]->fname)) {
		unique = 0;
		break;
	    }
	}

	if (unique) {
	    break;
	} 
    }
}

/* Callback for a command on the pattern "gname <- plotspec".
   Note @gname may contain non-ASCII characters (which will be 
   in UTF-8). So we'd best not use @gname to construct a filename
   for the graph in case we're on a non-UTF-8 system.
*/

int cli_add_graph_to_session (const char *fname, const char *gname,
			      GretlObjType type)
{
    SESSION_GRAPH *orig;
    char shortname[MAXSAVENAME];
    char grpath[MAXLEN];
    int replace = 0;
    
    errno = 0;

    if (!session_dir_ok()) {
	errbox(_("Failed to copy graph file"));
	return ADD_OBJECT_FAIL;
    }

    gretl_chdir(gretl_dotdir());

    orig = get_session_graph_by_name(gname);

    if (orig != NULL) {
	/* replacing */
	session_file_make_path(grpath, orig->fname);
	replace = 1;
    } else {
	/* ensure unique filename */
	make_graph_filename(shortname);
	session_file_make_path(grpath, shortname);
    }

    if (copyfile(fname, grpath)) {
	errbox(_("Failed to copy graph file"));
	return ADD_OBJECT_FAIL;
    } 

    /* we copied the plot commands file, @fname, into the
       session directory; now delete the original */
    gretl_remove(fname);

    if (replace) {
	return replace_session_graph(orig, NULL, type);
    } else {
	return real_add_graph_to_session(shortname, gname, type);
    }
}

void *get_session_object_by_name (const char *name, GretlObjType *type)
{
    int i;

    for (i=0; i<session.nmodels; i++) {
	if (!strcmp(name, session.models[i]->name)) {
	    *type = session.models[i]->type;
	    return session.models[i]->ptr;
	}
    }

    for (i=0; i<session.ngraphs; i++) {
	if (!strcmp(name, session.graphs[i]->name)) {
	    *type = GRETL_OBJ_GRAPH;
	    return session.graphs[i];
	}
    }

    for (i=0; i<session.ntexts; i++) {
	if (!strcmp(name, session.texts[i]->name)) {
	    *type = GRETL_OBJ_TEXT;
	    return session.texts[i];
	}
    }

    return NULL;
}

static SESSION_MODEL *get_session_model_by_data (const void *ptr)
{
    int i;

    for (i=0; i<session.nmodels; i++) {
	if (ptr == session.models[i]->ptr) {
	    return session.models[i];
	}
    }

    return NULL;
}

static void delete_icon_for_data (void *data)
{
    gui_obj *gobj = get_gui_obj_by_data(data);

    if (gobj != NULL) {
	/* icon is shown: delete it */
	session_delete_icon(gobj);
    }
}

/* Callback (indirect) from libgretl for a model created via 
   script command. When this function is called, the model
   in question has already been stacked; it is just a matter
   of syncing the GUI session with the model stack state.
*/

int add_model_to_session_callback (void *ptr, GretlObjType type)
{
    SESSION_MODEL *targ = NULL;
    char *name = gretl_object_get_name(ptr, type);
    int ret = 0;

    /* are we replacing a session model's content? */
    targ = get_session_model_by_name(name); 

    if (targ != NULL) {
	windata_t *vwin = get_viewer_for_data(targ->ptr);

	if (vwin != NULL) {
	    /* current model window is "orphaned" */
	    gretl_viewer_destroy(vwin);
	}

	targ->ptr = ptr;
	targ->type = type;
	mark_session_changed();
    } else {
	ret = add_model_to_session(ptr, name, type);
    }

    return ret;
}

static int model_type_from_vwin (windata_t *vwin)
{
    if (vwin->role == SYSTEM) {
	return GRETL_OBJ_SYS;
    } else if (vwin->role == VAR || vwin->role == VECM) {
	return GRETL_OBJ_VAR;
    } else {
	return GRETL_OBJ_EQN;
    }
}

static int close_on_add (GtkAction *action)
{
    if (action == NULL) {
	return 1;
    } else {
	return (strstr(gtk_action_get_name(action), "Close") != NULL);
    }
}

void model_add_as_icon (GtkAction *action, gpointer p)
{
    windata_t *vwin = (windata_t *) p;
    void *ptr = vwin->data;
    const char *name;
    int type;
    int err = 0;

    if (ptr == NULL) {
	return;
    }

#if SESSION_DEBUG
    fprintf(stderr, "model_add_as_icon: ptr = %p\n", ptr);
#endif

    if (get_session_model_by_data(ptr)) {
	/* "can't happen" */
	if (close_on_add(action)) {
	    gretl_viewer_destroy(vwin);
	} else {
	    infobox(_("Model is already saved"));
	}
	return;
    }
    
    type = model_type_from_vwin(vwin);
    name = gretl_object_get_name(ptr, type);

    if (name != NULL && *name == '\0') {
	/* since we didn't get a match by data above, we
	   need to ensure that this model is given a
	   unique name */
	gretl_object_compose_unique_name(ptr, type);
	name = gretl_object_get_name(ptr, type);
    }

    if (name == NULL) {
	nomem();
	return;
    }    

    err = add_model_to_session(ptr, name, type);

    if (!err) {
	mark_session_changed();
	if (close_on_add(action)) {
	    gretl_viewer_destroy(vwin);
	} else {
	    set_model_save_state(vwin, FALSE);
	}
    } 	
}

void bundle_add_as_icon (GtkAction *action, gpointer p)
{
    windata_t *vwin = (windata_t *) p;
    gretl_bundle *bundle = vwin->data;
    char vname[VNAMELEN];
    gchar *blurb;
    int *pshow, show = 0;
    int resp;

    sprintf(vname, "bundle%d", ++session_bundle_count);
    blurb = g_strdup_printf("Save bundle\nName (max. %d characters):",
			    VNAMELEN - 1);
    pshow = (iconlist == NULL)? &show : NULL;
    resp = object_name_entry_dialog(vname, GRETL_TYPE_BUNDLE, 
				    blurb, pshow, vwin->main);
    g_free(blurb);

    if (!canceled(resp)) { 
	int err = user_var_add(vname, GRETL_TYPE_BUNDLE, bundle);
	int flipit = 1;

	if (err) {
	    gui_errmsg(err);
	} else {
	    if (show) {
		view_session();
	    } else if (autoicon_on()) {
		auto_view_session();
	    }
	    mark_session_changed();
	    if (close_on_add(action)) {
		gtk_widget_destroy(vwin->main);
		flipit = 0;
	    }
	}

	if (flipit) {
	    flip(vwin->ui, "/menubar/File/SaveAsIcon", FALSE);
	    flip(vwin->ui, "/menubar/File/SaveAndClose", FALSE);
	}
    }   
}

static void
session_name_from_session_file (char *sname, const char *fname)
{
    const char *p = strrchr(fname, SLASH);
    char *q;

    if (p != NULL) {
	strcpy(sname, p + 1);
    } else {
	strcpy(sname, fname);
    }

    q = strstr(sname, ".gretl");
    if (q != NULL && strlen(q) == 6) {
	*q = '\0';
    }

    fprintf(stderr, "session_name_from_session_file: %s -> %s\n",
	    fname, sname);
}

/* remedial action in case of mis-coded filename */

static int get_session_dataname (char *fname, struct sample_info *sinfo)
{
    struct dirent *dirent;
    DIR *dir;
    FILE *fp;
    gchar *tmp;
    int n, err = E_FOPEN;

    tmp = g_strdup(session.dirname);
    n = strlen(tmp);

    if (tmp[n-1] == '/' || tmp[n-1] == '\\') {
	tmp[n-1] = '\0';
    }

    dir = opendir(tmp);

    if (dir != NULL) {
	while ((dirent = readdir(dir)) != NULL) {
	    if (has_suffix(dirent->d_name, ".gdt")) {
		session_file_make_path(fname, dirent->d_name);
		fp = gretl_fopen(fname, "r");
		if (fp != NULL) {
		    fclose(fp);
		    err = 0;
		} 
		break;
	    }
	}	
	closedir(dir);
    }

    g_free(tmp);

    return err;
}

static void sinfo_init (struct sample_info *sinfo)
{
    strcpy(sinfo->datafile, "data.gdt");
    sinfo->t1 = 0;
    sinfo->t2 = 0;
    sinfo->mask = NULL;
    sinfo->restriction = NULL;
    sinfo->seed = 0;
    sinfo->resample_n = 0;
}

static void sinfo_free_data (struct sample_info *sinfo)
{
    if (sinfo->mask != NULL) {
	free(sinfo->mask);
    }
    if (sinfo->restriction != NULL) {
	free(sinfo->restriction);
    }
}

static int set_session_dirname (const char *zdirname)
{
    char test[MAXLEN];
    FILE *fp;
    int err = 0;

    g_return_val_if_fail(zdirname != NULL, 1);

    strcpy(session.dirname, zdirname);
    sprintf(test, "%s%csession.xml", session.dirname, SLASH);
    fp = gretl_fopen(test, "r");

    if (fp == NULL) {
	err = E_FOPEN;
    } else {
	fclose(fp);
    }

    return err;
}

/* note: the name of the file to be opened is in the global var
   'tryfile' */

gboolean do_open_session (void)
{
    struct sample_info sinfo;
    char xmlname[MAXLEN]; /* path to master session XML file */
    char gdtname[MAXLEN]; /* path to session data file */
    char fname[MAXLEN];   /* multi-purpose temp variable */
    gchar *zdirname = NULL;
    GError *gerr = NULL;
    FILE *fp;
    int nodata = 0;
    int err = 0;

    sinfo_init(&sinfo);

    fp = gretl_fopen(tryfile, "r");
    if (fp != NULL) {
	fclose(fp);
    } else {
	file_read_errbox(tryfile);
	delete_from_filelist(FILE_LIST_SESSION, tryfile);
	return FALSE;
    }

    /* close existing session, if any, and initialize */
    close_session(OPT_NONE);

    fprintf(stderr, I_("\nReading session file %s\n"), tryfile);

    gretl_chdir(gretl_dotdir());
    err = gretl_unzip_session_file(tryfile, &zdirname, &gerr);

    if (gerr != NULL && err == 0) {
	err = 1;
    } 

    if (err) {
	if (gerr != NULL) {
	    errbox(gerr->message);
	    g_error_free(gerr);
	} else {
	    /* FIXME more explicit error message */
	    file_read_errbox(tryfile);
	}
	g_free(zdirname);
	goto bailout;
    } 

    session_name_from_session_file(session.name, tryfile);

    err = set_session_dirname(zdirname);
    if (err) {
	g_free(zdirname);
	fprintf(stderr, "Failed on set_session_dirname\n");
	file_read_errbox("session.xml");
	goto bailout;
    } else {
	fprintf(stderr, "set_session_dirname: '%s', OK\n", zdirname);
    }

    g_free(zdirname);
    session_file_make_path(xmlname, "session.xml");

    /* try getting the name of the session data file first */
    err = get_session_datafile_name(xmlname, &sinfo, &nodata);

    if (err) {
	fprintf(stderr, "Failed on read_session_xml: err = %d\n", err);
	file_read_errbox("session.xml");
	goto bailout;
    }

    if (!nodata) {
	/* construct path to session data file */
	session_file_make_path(datafile, sinfo.datafile);
	fp = gretl_fopen(datafile, "r");

	if (fp != NULL) {
	    /* OK, write good name into gdtname */
	    fprintf(stderr, "got datafile name '%s'\n", datafile);
	    strcpy(gdtname, datafile);
	    fclose(fp);
	} else {
	    /* try remedial action, transform filename? */
	    fprintf(stderr, "'%s' : not found, trying to fix\n", datafile);
	    err = get_session_dataname(gdtname, &sinfo);
	}

	if (!err) {
	    err = gretl_read_gdt(gdtname, dataset, OPT_B, NULL);
	}

	if (err) {
	    /* FIXME more explicit error message? */
	    file_read_errbox(sinfo.datafile);
	    goto bailout;
	} else {
	    fprintf(stderr, "Opened session datafile '%s'\n", gdtname);
	    data_status = USER_DATA;
	}
    }

    /* having opened the data file (or not, if there's none), get the 
       rest of the info from session.xml 
    */    
    err = read_session_xml(xmlname, &sinfo);
    if (err) {
	fprintf(stderr, "Failed on read_session_xml: err = %d\n", err);
	file_read_errbox("session.xml");
	goto bailout;
    }

    err = deserialize_user_vars(session.dirname);

    session_file_make_path(fname, "functions.xml");
    err = maybe_read_functions_file(fname);

    session_file_make_path(fname, "settings.inp");
    err = maybe_read_settings_file(fname);

    if (!nodata) {
	if (sinfo.resample_n > 0) {
	    err = dataset_resample(dataset, sinfo.resample_n, sinfo.seed); 
	} else if (sinfo.mask != NULL) {
	    err = restrict_sample_from_mask(sinfo.mask, dataset,
					    OPT_NONE);
	    if (!err) {
		dataset->restriction = sinfo.restriction;
		sinfo.restriction = NULL;
	    }
	}

	if (err) {
	    errbox(_("Couldn't set sample"));
	    goto bailout;
	}

	dataset->t1 = sinfo.t1;
	dataset->t2 = sinfo.t2;

	register_data(OPENED_VIA_SESSION);

	if (sinfo.mask != NULL) {
	    set_sample_label(dataset);
	}

	sinfo_free_data(&sinfo);
    }

    set_main_window_title(session.name, FALSE);

 bailout:

    if (err) {
	delete_from_filelist(FILE_LIST_SESSION, tryfile);
    } else {
	strcpy(sessionfile, tryfile);
	mkfilelist(FILE_LIST_SESSION, sessionfile);

	session.status = SESSION_OPEN;

	/* sync gui with session */
	session_menu_state(TRUE);

	view_session();
	mark_session_saved();
	session_switch_log_location(LOG_OPEN);

	if (session.show_notes) {
	    edit_session_notes();
	}
    }

    return !err;
}

void verify_clear_data (void)
{
    if (dataset_locked()) {
	return;
    }

    if (yes_no_dialog ("gretl",                      
		       _("Clearing the data set will end\n"
			 "your current session.  Continue?"), 
		       0) != GRETL_YES) {
	return;
    }

    close_session(OPT_NONE); /* FIXME opt */
}

static void remove_session_dir (void)
{
    const char *dotdir = gretl_dotdir();
    char *fullpath = g_strdup_printf("%s%s", dotdir,
				     session.dirname);

    gretl_chdir(dotdir);
    gretl_deltree(fullpath);
    g_free(fullpath);
}

void session_init (void)
{
    session.models = NULL;
    session.graphs = NULL;
    session.texts = NULL;
    session.notes = NULL;

    session.status = 0; /* note: neither changed nor saved */
    session.nmodels = 0;
    session.ngraphs = 0;
    session.ntexts = 0;

    *session.name = '\0';
    *session.dirname = '\0';

    set_user_var_callback(gui_user_var_callback);
}

void free_session (gretlopt opt)
{
    int i;

    if (session.models) {
	for (i=0; i<session.nmodels; i++) {
	    free_session_model(session.models[i]);
	}
	free(session.models);
	session.models = NULL;
    }
    session.nmodels = 0;

    /* FIXME OPT_P ? Maybe we should try to preserve
       graphs and texts, though this would require
       that we don't trash the session directory, and
       instead remove only the model files. Perhaps
       this is just too complicated.
    */

    if (1 || !(opt & OPT_P)) {
	if (session.graphs) {
	    for (i=0; i<session.ngraphs; i++) {
		free(session.graphs[i]);
	    }
	    free(session.graphs);
	    session.graphs = NULL;
	}
	session.ngraphs = 0;

	if (session.texts) {
	    for (i=0; i<session.ntexts; i++) {
		free_session_text(session.texts[i]);
	    }	
	    free(session.texts);
	    session.texts = NULL;
	}
	session.ntexts = 0;

	if (session.notes) {
	    free(session.notes);
	    session.notes = NULL;
	}
    }

    *session.name = '\0';

    if (*session.dirname != '\0') {
	remove_session_dir();
	*session.dirname = '\0';
    }

#if defined(HAVE_FLITE) || defined(WIN32_SAPI)
    stop_talking();
#endif
}

int highest_numbered_variable_in_session (void)
{
    GretlObjType type;
    void *ptr;
    int i, mvm, vmax = 0;

    if (session.models == NULL) {
	return 0;
    }

    for (i=0; i<session.nmodels; i++) {
	ptr = session.models[i]->ptr;
	if (ptr == NULL) {
	    continue;
	}
	type = session.models[i]->type;
	if (type == GRETL_OBJ_EQN) {
	    mvm = highest_numbered_var_in_model((MODEL *) ptr, dataset);
	    if (mvm > vmax) {
		vmax = mvm;
	    }
	} else if (type == GRETL_OBJ_VAR) {
	    mvm = gretl_VAR_get_highest_variable((GRETL_VAR *) ptr);
	    if (mvm > vmax) {
		vmax = mvm;
	    }
	} else if (type == GRETL_OBJ_SYS) {
	    mvm = highest_numbered_var_in_system((equation_system *) ptr, 
						 dataset);
	    if (mvm > vmax) {
		vmax = mvm;
	    }
	}
    }

    return vmax;
}

int session_file_is_open (void)
{
    return (*sessionfile != '\0');
}

void gui_clear_dataset (void)
{
    *datafile = 0;

    if (dataset->Z != NULL) {
	free_Z(dataset);
	dataset->Z = NULL;
    } 

    clear_datainfo(dataset, CLEAR_FULL);

    clear_varlist(mdata->listbox);
    clear_sample_label();

    data_status = 0;
    orig_vars = 0;
    dataset_menubar_state(FALSE);
}

static void session_clear_data (DATASET *pdinfo)
{
    gui_restore_sample(pdinfo);
    gui_clear_dataset();

    /* clear protected model */
    clear_model(model);

    free_command_stack(); 
    set_model_count(0);
    lib_cmd_destroy_context();
}

void close_session (gretlopt opt)
{
    int logcode = LOG_NULL;
    int iview = 0;

#if SESSION_DEBUG
    fprintf(stderr, "close_session: starting cleanup\n");
#endif

    if (dataset != NULL && dataset->v > 0) {
	logcode = LOG_CLOSE;
	session_clear_data(dataset); 
    }
 
    free_session(opt);

    clear_model_table(1, NULL);
    clear_graph_page(1);

    session_menu_state(FALSE);
    *scriptfile = '\0';
    *sessionfile = '\0';

    if (iconview != NULL) {
	iview = 1;
	gtk_widget_destroy(iconview);
    }

    session.status = 0; /* not saved, changed or open */
    session.show_notes = 0;
    commands_recorded = 0;

    close_session_windows();
    selector_cleanup();
    function_call_cleanup();
    edit_dialog_special_get_text(NULL);

    if (opt & OPT_P) {
	/* preserve non-dataset items */
	libgretl_session_cleanup(SESSION_CLEAR_DATASET);
    } else {
	libgretl_session_cleanup(SESSION_CLEAR_ALL);
    }

    session_graph_count = 0;
    session_bundle_count = 0;
    reset_plot_count();

    set_session_log(NULL, logcode);

    if (iview && have_session_objects()) {
	dataset_menubar_state(FALSE);
	view_session();
    }
}

static void relpath_from_fname (char *path, const char *fname)
{
    const char *p;

    strcpy(path, ".");

    p = (*fname == SLASH)? fname + 1 : fname;
    p = strrchr(p, SLASH);

    if (p != NULL) {
	strcat(path, p + 1);
    } else {
	strcat(path, fname);
    }
}

/* dump the current dataset into the session dir */

static int real_save_session_dataset (const char *dname)
{
    char tmpname[MAXLEN];
    char *mask = NULL;
    int save_t1 = dataset->t1;
    int save_t2 = dataset->t2;
    int write_err = 0;
    int err = 0;

    /* we need to retrieve and save the full version of the dataset */

    if (complex_subsampled()) {
	mask = copy_datainfo_submask(dataset, &err);
	if (!err && dataset_is_resampled(dataset)) {
	    /* can't happen? */
	    mask = NULL;
	}
    }

    if (!err) {
	err = restore_full_sample(dataset, NULL);
    }

    if (!err) {
	session_file_make_path(tmpname, dname);
	write_err = gretl_write_gdt(tmpname, NULL, dataset, 
				    OPT_NONE, 1);
    }

    if (mask != NULL) {
	/* reset the prior subsample */
	if (!err) {
	    err = restrict_sample_from_mask(mask, dataset, OPT_NONE);
	}
	free(mask);
    }

    dataset->t1 = save_t1;
    dataset->t2 = save_t2;

    if (!err) {
	err = write_err;
    }

    if (!err) {
	/* flag the fact that the data are saved */
	data_status &= ~MODIFIED_DATA;
    }
    
    return err;
}

static const char *unpath (const char *fname)
{
    int i, n = strlen(fname);

    for (i=n-1; i>=0; i--) {
	if (fname[i] == '/') {
	    return fname + i + 1;
	}
#ifdef G_OS_WIN32
	if (fname[i] == '\\') {
	    return fname + i + 1;
	}
#endif
    }

    return fname;
}

/* called in the context of a re-opened session */

int save_session_dataset (void)
{
    int err = real_save_session_dataset(unpath(datafile));

    if (err) {
	gui_errmsg(err);
    } 

    return err;
}

static void make_session_dataname (char *datname)
{
    int changed = 0;

    if (*datafile != '\0') {
	char *p;

	strcpy(datname, unpath(datafile));
	p = strrchr(datname, '.');
	if (p == NULL) {
	    strcat(datname, ".gdt");
	    changed = 1;
	} else if (strcmp(p, ".gdt")) {
	    strcpy(p, ".gdt");
	    changed = 1;
	}
    } else {
	strcpy(datname, "data.gdt");
	changed = 1;
    }

    if (changed) {
	GtkWidget *dlabel;

	dlabel = g_object_get_data(G_OBJECT(mdata->main), "dlabel");
	if (dlabel != NULL) {
	    gtk_label_set_text(GTK_LABEL(dlabel), datname);
	}
    }
}

#define SAVE_DEBUG 1

int save_session (char *fname) 
{
    char *dirbak = NULL;
    char datname[MAXLEN];
    char dirname[MAXLEN];
    int len, err = 0;
    int log_code = LOG_SAVE;
    GError *gerr = NULL;

    if (fname == NULL) {
	/* re-saving session 'as is' */
	fname = sessionfile;
    } 

#if SAVE_DEBUG
    if (fname == sessionfile) {
	fprintf(stderr, "save_session:\n sessionfile='%s'\n",
		sessionfile);
    } else {
	fprintf(stderr, "save_session as:\n current session='%s'\n"
		" save filename='%s'\n", sessionfile, fname);
    }
#endif

    if (!session_dir_ok()) {
	errbox("Couldn't make session directory");
	return 1;
    }

    /* organize directory and file names */
    relpath_from_fname(dirname, fname);
    len = strlen(fname);
    if (len > 6 && !strncmp(fname + len - 6, ".gretl", 6)) {
	dirname[strlen(dirname) - 6] = '\0';
    } else {
	strcat(fname, ".gretl");
    }

#if SAVE_DEBUG
    fprintf(stderr, " save dirname = '%s'\n", dirname);
    fprintf(stderr, " current session.dirname = '%s'\n", session.dirname);
    fprintf(stderr, " doing chdir to '%s'\n", gretl_dotdir());
#endif

    /* note: paths below are relative to this */
    err = gretl_chdir(gretl_dotdir());
    if (err) {
	fprintf(stderr, " chdir to dotdir failed\n");
	gui_errmsg(err);
	return 1;
    }

    if (strcmp(dirname, session.dirname)) {
	/* have to rename the session directory */
	maybe_suspend_session_log();
	log_code = LOG_SAVE_AS;
	dirbak = gretl_strdup(session.dirname);
#ifdef G_OS_WIN32
	err = win32_rename_dir(session.dirname, dirname);
#else
	err = gretl_rename(session.dirname, dirname);
#endif
	if (err) {
	    fprintf(stderr, " failed to rename session dir\n");
	    gui_errmsg(err);
	} else {
	    fprintf(stderr, " renamed session dir OK\n");
	    strcpy(session.dirname, dirname);
	}
    }  

    if (!err) {
	*datname = '\0';
	if (data_status) {
	    make_session_dataname(datname);
	} else {
	    strcpy(datname, "none");
	}

	err = write_session_xml(datname);
#if SAVE_DEBUG
	fprintf(stderr, " write_session_xml: err = %d\n", err);
#endif
    }

    if (!err && data_status) {
	err = real_save_session_dataset(datname);
#if SAVE_DEBUG
	fprintf(stderr, " real_save_session_dataset: err = %d\n", err);
#endif
    }

    if (!err) {
	session_switch_log_location(log_code);
    }
    
    if (!err) {
	/* make zipfile containing session files */
	err = gretl_make_zipfile(fname, dirname, &gerr);
	fprintf(stderr, " gretl_make_zipfile: err = %d\n", err);

	if (gerr != NULL && !err) {
	    err = 1;
	}

	if (err) {
	    if (gerr != NULL) {
		errbox(gerr->message);
		g_error_free(gerr);
	    }
	} else {
	    mkfilelist(FILE_LIST_SESSION, fname);
	    if (fname != sessionfile) {
		session_name_from_session_file(session.name, fname);
		strcpy(sessionfile, fname);
		data_status |= SESSION_DATA; /* FIXME? */
		set_sample_label(dataset);
	    }
	    mark_session_saved();
	}
    }

    if (dirbak != NULL) {
	if (err) {
	    /* restore original name on error */
	    strcpy(session.dirname, dirbak);
	}
	free(dirbak);
    }

    fprintf(stderr, "save_session; returning %d\n", err);

    return err;
}

void session_notes_callback (GtkWidget *w, gpointer p)
{
    windata_t *vwin = (windata_t *) p;
    const char *opts[] = {
	N_("Display notes on opening session file")
    };
    int active[] = {0};
    int resp;

    active[0] = session.show_notes;

    resp = checks_only_dialog("gretl", NULL, opts, 1,
			      active, 0, vwin->main);

    if (resp >= 0 && session.show_notes != active[0]) {
	session.show_notes = active[0];
	mark_session_changed();
    }
}

void save_session_callback (GtkAction *action)
{
    int as_is = 0;

    if (session_file_is_open() && action != NULL) {
	const gchar *s = gtk_action_get_name(action);

	if (!strcmp(s, "SaveSession")) {
	    as_is = 1;
	}
    }

    if (as_is) {
	save_session(sessionfile);
    } else {
	file_selector(SAVE_SESSION, FSEL_DATA_NONE, NULL);
    }
}

int save_session_commands (char *fname)
{
    FILE *fp = gretl_fopen(fname, "w");
    int err = 0;

    if (fp == NULL) {
	file_write_errbox(fname);
	err = E_FOPEN;
    } else {
	gchar *s = get_logfile_content(&err);

	if (err) {
	    gui_errmsg(err);
	} else {
	    fputs(s, fp);
	    g_free(s);
	}

	fclose(fp);
    }

    return err;
}

static char *model_cmd_str (MODEL *pmod)
{
    char *str = NULL;

    if (pmod->ci == MLE || pmod->ci == GMM || pmod->ncoeff > 10 ||
	pmod->list[0] > 10) {
	return NULL;
    }

    str = malloc(MAXLEN);
    if (str == NULL) {
	return NULL;
    }

    sprintf(str, "%s ", gretl_command_word(pmod->ci));

    if (pmod->ci == AR) {
        model_list_to_string(pmod->arinfo->arlist, str);
        strcat(str, "; ");
    }

    model_list_to_string(pmod->list, str); 

    return str;
}

static gchar *graph_str (SESSION_GRAPH *graph)
{
    char tmp[MAXLEN];
    FILE *fp;
    gchar *buf = NULL;

    session_file_make_path(tmp, graph->fname);
    fp = gretl_fopen(tmp, "r");

    /* FIXME boxplots */

    if (fp != NULL) {
	char line[128], title[64];
	char xlabel[24], ylabel[24];
	int gottitle = 0;
	int gotxy = 0;

	while (fgets(line, sizeof line, fp)) {
	    if (strstr(line, "# timeseries") || strstr(line, "# frequency") ||
		!strncmp(line, "plot", 4)) {
		break;
	    } else if (sscanf(line, "set title '%63[^']", title) == 1) {
		gottitle = 1;
		break;
	    } else if (sscanf(line, "set xlabel '%23[^']", xlabel) == 1) {
		gotxy++;
	    } else if (sscanf(line, "set ylabel '%23[^']", ylabel) == 1) {
		gotxy++;
	    } 
	}

	if (gottitle) {
	    /* FIXME? encoding */
	    buf = my_locale_to_utf8(title);
	} else if (gotxy == 2) {
	    char *s = g_strdup_printf("%s %s %s", ylabel, _("versus"), xlabel);

	    if (s != NULL) {
		buf = my_locale_to_utf8(s);
		free(s);
	    }
	}

	fclose(fp);
    }

    return buf;
}

static int maybe_raise_object_window (gpointer data)
{
    windata_t *vwin = get_viewer_for_data(data);

    if (vwin != NULL) {
	gretl_viewer_present(vwin);
	return 1;
    } else {
	return 0;
    }
}

static int display_session_model (SESSION_MODEL *sm)
{ 
    PRN *prn;

    if (maybe_raise_object_window(sm->ptr)) {
	return 0;
    }  

    if (sm->type != GRETL_OBJ_SYS && bufopen(&prn)) {
	return 1;
    }

    if (sm->type == GRETL_OBJ_EQN) {
	MODEL *pmod = (MODEL *) sm->ptr;

	printmodel(pmod, dataset, OPT_NONE, prn);
	view_model(prn, pmod, sm->name);
    } else if (sm->type == GRETL_OBJ_VAR) {
	GRETL_VAR *var = (GRETL_VAR *) sm->ptr;

	gretl_VAR_print(var, dataset, OPT_NONE, prn);
	view_buffer(prn, 78, 450, sm->name, var->ci, var);
    } else if (sm->type == GRETL_OBJ_SYS) {
	equation_system *sys = (equation_system *) sm->ptr;

	edit_dialog(ESTIMATE, sm->name, NULL, NULL, 
		    do_saved_eqn_system, sys, 
		    VARCLICK_NONE, iconview); 
    }

    return 0;
}

/* callback used in objectsave.c */

void session_model_callback (void *ptr, int action)
{
    SESSION_MODEL *mod = get_session_model_by_data(ptr);

    if (mod == NULL) {
	return;
    }
    
    if (action == OBJ_ACTION_SHOW) {
	display_session_model(mod);
    } else if (action == OBJ_ACTION_FREE) {
	delete_icon_for_data(mod);
	real_delete_model_from_session(mod);
    } 
}	

static void open_matrix (gui_obj *obj)
{
    user_var *u = (user_var *) obj->data;
    const char *name = user_var_get_name(u);

    edit_user_matrix_by_name(name, iconview);
}

static void open_bundle (gui_obj *obj)
{
    user_var *u = (user_var *) obj->data;
    const char *name = user_var_get_name(u);
    gretl_bundle *b = user_var_get_value(u);
    PRN *prn = NULL;
    int done = 0;

    if (maybe_raise_object_window(b)) {
	return;
    }

    if (gretl_bundle_get_n_keys(b) == 0) {
	warnbox(_("Bundle is empty"));
	return;
    }

    if (bufopen(&prn)) {
	return;
    }

    done = try_exec_bundle_print_function(b, prn);

    if (!done) {
 	gretl_bundle_print(b, prn);
    }

    view_buffer(prn, 80, 400, name, VIEW_BUNDLE, b);
}

static void open_gui_text (gui_obj *obj)
{ 
    SESSION_TEXT *text = (SESSION_TEXT *) obj->data;
    PRN *prn;

    prn = gretl_print_new_with_buffer(g_strdup(text->buf));

    if (prn != NULL) { 
	view_buffer(prn, 80, 400, obj->name, INFO, NULL);
    }
}

static int real_delete_model_from_session (SESSION_MODEL *model)
{
    int nm = session.nmodels - 1;

    if (nm == 0) {
	free_session_model(session.models[0]);
	free(session.models);
	session.models = NULL;
    } else {
	SESSION_MODEL **mods;
	int i, j = 0;

	for (i=0; i<session.nmodels; i++) {
	    if (session.models[i]->ptr == model->ptr) {
		free_session_model(session.models[i]);
		j = i;
		break;
	    }
	}

	for (i=j; i<nm; i++) {
	    session.models[i] = session.models[i+1];
	}

	mods = myrealloc(session.models, nm * sizeof *mods);
	if (mods != NULL) {
	    session.models = mods;
	}
    }

    session.nmodels = nm;
    mark_session_changed();

    return 0;
}

static int real_delete_text_from_session (SESSION_TEXT *junk)
{
    int nt = session.ntexts;

    if (nt == 1) {
	free_session_text(session.texts[0]);
	free(session.texts);
	session.texts = NULL;
    } else {
	SESSION_TEXT **pptext;
	int i, j;

	pptext = mymalloc((nt - 1) * sizeof *pptext);
	if (pptext == NULL) {
	    return 1;
	}
	j = 0;
	for (i=0; i<nt; i++) {
	    if (session.texts[i] != junk) {
		pptext[j++] = session.texts[i];
	    } else {
		free_session_text(session.texts[i]);
	    }
	}
	free(session.texts);
	session.texts = pptext;
    }

    session.ntexts = nt - 1;
    mark_session_changed();

    return 0;
}

static void remove_session_graph_file (const char *gfname)
{
    char fname[MAXLEN];

    gretl_chdir(gretl_dotdir());
    session_file_make_path(fname, gfname);
    gretl_remove(fname);
}

static int real_delete_graph_from_session (SESSION_GRAPH *junk)
{
    int ng = session.ngraphs;

    if (ng == 1) {
	remove_session_graph_file(session.graphs[0]->fname);
	free(session.graphs[0]);
	free(session.graphs);
	session.graphs = NULL;	
    } else {
	SESSION_GRAPH **ppgr;
	int i, j, done = 0;

	for (i=0; i<ng && !done; i++) {
	    if (!strcmp(session.graphs[i]->name, junk->name)) {
		remove_session_graph_file(session.graphs[i]->fname);
		free(session.graphs[i]);
		for (j=i; j<ng-1; j++) {
		    session.graphs[j] = session.graphs[j+1];
		}
		done = 1;
	    }
	}

	if (done) {
	    ppgr = myrealloc(session.graphs, (ng - 1) * sizeof *ppgr);
	    if (ppgr == NULL) {
		return 1;
	    } 	
	    session.graphs = ppgr;
	}
    }

    session.ngraphs = ng - 1;
    mark_session_changed();

    return 0;
}

static int delete_session_object (gui_obj *obj)
{
    if (obj->sort == GRETL_OBJ_EQN || obj->sort == GRETL_OBJ_VAR ||
	obj->sort == GRETL_OBJ_SYS) { 
	real_delete_model_from_session(obj->data);
    } else if (obj->sort == GRETL_OBJ_GRAPH || obj->sort == GRETL_OBJ_PLOT) { 
	real_delete_graph_from_session(obj->data);
    } else if (obj->sort == GRETL_OBJ_TEXT) { 
	real_delete_text_from_session(obj->data);
    } else if (obj->sort == GRETL_OBJ_MATRIX ||
	       obj->sort == GRETL_OBJ_BUNDLE) {
	user_var_delete(obj->data);
    }

    session_delete_icon(obj);
    mark_session_changed();

    return 0;
}

/* run a sanity check on deleting a session object (e.g.
   model, graph) before proceeding */

static void maybe_delete_session_object (gui_obj *obj)
{
    int busy = 0;

    if (obj->sort == GRETL_OBJ_GRAPH || obj->sort == GRETL_OBJ_PLOT) {
	SESSION_GRAPH *graph = (SESSION_GRAPH *) obj->data;
	char fullname[MAXLEN];

	session_file_make_path(fullname, graph->fname);
	if (get_window_for_plot(fullname) || 
	    get_editor_for_file(fullname)) {
	    busy = 1;
	}
    } else {
	gpointer p = NULL;

	if (obj->sort == GRETL_OBJ_EQN || obj->sort == GRETL_OBJ_SYS || 
	    obj->sort == GRETL_OBJ_VAR) {
	    SESSION_MODEL *mod = obj->data;

	    p = mod->ptr;
	} else if (obj->sort == GRETL_OBJ_BUNDLE) {
	    p = user_var_get_value((user_var *) obj->data);
	} else {
	    p = obj->data;
	}

	if (obj->sort == GRETL_OBJ_MATRIX) {
	    busy = (p != NULL && get_window_for_data(p) != NULL);
	} else {
	    busy = (p != NULL && get_viewer_for_data(p) != NULL);
	}
    }

    if (busy) {
	warnbox(_("%s: please close this object's window first"),
		obj->name);
    } else {	    
	gchar *msg = g_strdup_printf(_("Really delete %s?"), obj->name);

	if (yes_no_dialog(_("gretl: delete"), msg, 0) == GRETL_YES) {
	    delete_session_object(obj);
	}
	g_free(msg);
    }
}

#if 0 /* FIXME? */
static gui_obj *get_gui_obj_by_name_and_sort (const char *name,
					      int sort)
{
    GList *mylist = g_list_first(iconlist);
    gui_obj *obj = NULL;

    while (mylist != NULL) {
	obj = (gui_obj *) mylist->data;
	if (!strcmp(name, obj->name) && obj->sort == sort) {
	    return obj;
	}
	mylist = mylist->next;
    }

    return NULL;
}
#endif

static gui_obj *get_gui_obj_by_data (void *targ)
{
    GList *mylist = g_list_first(iconlist);
    gui_obj *obj = NULL;

    while (mylist != NULL) {
	obj = (gui_obj *) mylist->data;
	if (obj->data == targ) {
	    return obj;
	}
	mylist = mylist->next;
    }

    return NULL;
}

static GretlObjType get_obj_type (GretlType type)
{
    if (type == GRETL_TYPE_MATRIX) {
	return GRETL_OBJ_MATRIX;
    } else if (type == GRETL_TYPE_BUNDLE) {
	return GRETL_OBJ_BUNDLE;
    } else {
	return GRETL_TYPE_NONE;
    }
}

/* called from DELEET case in library.c */

int session_user_var_destroy_by_name (const char *name, 
				      GretlObjType type)
{
    user_var *u = get_user_var_by_name(name);
    int err;

    if (u == NULL) {
	err = E_UNKVAR;
    } else {
	maybe_close_window_for_user_var(u, type);
	if (iconlist != NULL) {
	    gui_obj *obj = get_gui_obj_by_data(u);

	    session_delete_icon(obj);
	}
	err = user_var_delete(u);
    } 
    
    return err;
}

static void rename_session_graph (SESSION_GRAPH *graph, const char *newname)
{
    int i;

    for (i=0; i<session.ngraphs; i++) {
	if (!strcmp(session.graphs[i]->name, graph->name)) { 
	    session.graphs[i]->name[0] = '\0';
	    strncat(session.graphs[i]->name, newname, MAXSAVENAME - 1);
	    break;
	}
    }
}

static void maybe_sync_model_window_name (SESSION_MODEL *sm)
{
    windata_t *vwin = get_viewer_for_data(sm->ptr);

    if (vwin != NULL) {
	gchar *title = g_strdup_printf("gretl: %s", sm->name);

	gretl_viewer_set_title(vwin, title);
	g_free(title);
    }
}

static int rename_session_object (gui_obj *obj, const char *newname)
{
    int err = 0;

    if (obj->sort == GRETL_OBJ_EQN || obj->sort == GRETL_OBJ_SYS || 
	obj->sort == GRETL_OBJ_VAR) { 
	SESSION_MODEL *sm;

	sm = get_session_model_by_name(newname);
	if (sm != NULL) {
	    err = 1;
	} else {
	    sm = obj->data;
	    gretl_object_rename(sm->ptr, sm->type, newname);
	    *sm->name = '\0';
	    strncat(sm->name, newname, MAXSAVENAME - 1);
	    maybe_sync_model_window_name(sm);
	}
    } else if (obj->sort == GRETL_OBJ_GRAPH || obj->sort == GRETL_OBJ_PLOT) {
	SESSION_GRAPH *sg;

	sg = get_session_graph_by_name(newname);
	if (sg != NULL) {
	    err = 1;
	} else {
	    sg = obj->data;
	    rename_session_graph(sg, newname);
	}
    } else if (obj->sort == GRETL_OBJ_MATRIX) {
	user_var_set_name(obj->data, newname);
    } else if (obj->sort == GRETL_OBJ_BUNDLE) {
	gretl_bundle_set_name(obj->data, newname);
    } else if (obj->sort == GRETL_OBJ_TEXT) {
	SESSION_TEXT *st;

	st = get_session_text_by_name(newname);
	if (st != NULL) {
	    err = 1;
	} else {
	    st = obj->data;
	    *st->name = '\0';
	    strncat(st->name, newname, MAXSAVENAME - 1);
	}
    }

    if (err) {
	errbox(_("'%s': there is already an object of this name"), newname);
    } else {
	free(obj->name);
	obj->name = g_strdup(newname);
    }

    return err;
}

static int copy_session_object (gui_obj *obj, const char *cpyname)
{
    void *oldp = NULL;
    void *p = NULL;
    int ptype = 0;
    int err = 0;

    /* Only graphs and matrices are supported for copying */

    if (obj->sort == GRETL_OBJ_GRAPH || obj->sort == GRETL_OBJ_PLOT) {
	oldp = get_session_graph_by_name(cpyname);
    } else if (obj->sort == GRETL_OBJ_MATRIX) {
	oldp = get_user_var_by_name(cpyname);
    } 

    if (oldp != NULL) {
	errbox(_("'%s': there is already an object of this name"), cpyname);
    } else if (obj->sort == GRETL_OBJ_GRAPH || obj->sort == GRETL_OBJ_PLOT) {
	SESSION_GRAPH *g0 = obj->data;

	errno = 0;
	if (!session_dir_ok()) {
	    err = 1;
	} else {
	    char fname1[MAXSAVENAME];
	    char path0[FILENAME_MAX];
	    char path1[FILENAME_MAX];

	    gretl_chdir(gretl_dotdir());
	    make_graph_filename(fname1);
	    session_file_make_path(path0, g0->fname);
	    session_file_make_path(path1, fname1);
	    err = copyfile(path0, path1);
	    if (!err) {
		p = session_append_graph(cpyname, fname1, g0->type);
		ptype = g0->type;
		err = (p == NULL);
	    }
	}
    } else if (obj->sort == GRETL_OBJ_MATRIX) {
	user_var *u = obj->data;

	err = copy_matrix_as(user_var_get_value(u), cpyname, 0);
	if (!err) {
	    p = get_user_var_by_name(cpyname);
	    ptype = GRETL_OBJ_MATRIX;
	    err = (p == NULL);
	}
    }

    if (!err) {
	session_add_icon(p, ptype, ICON_ADD_SINGLE);
    }

    return err;
}

static void copy_object_callback (GtkWidget *widget, dialog_t *dlg) 
{
    gui_obj *obj = (gui_obj *) edit_dialog_get_data(dlg);
    const gchar *cpyname;
    int err = 0;

    cpyname = edit_dialog_get_text(dlg);

    if (cpyname != NULL && *cpyname != '\0') {
	err = copy_session_object(obj, cpyname);
	if (err) {
	    errbox(_("Failed to copy object"));
	} else {
	    mark_session_changed();
	} 
    }

    if (!err) {
	edit_dialog_close(dlg);
    }
}

static void copy_object_dialog (gui_obj *obj) 
{
    int maxlen = MAXSAVENAME - 1;
    gchar *tmp;

    if (obj->sort != GRETL_OBJ_MATRIX &&
	obj->sort != GRETL_OBJ_GRAPH &&
	obj->sort != GRETL_OBJ_PLOT) {
	dummy_call();
    }

    if (obj->sort == GRETL_OBJ_MATRIX || obj->sort == GRETL_OBJ_BUNDLE) {
	maxlen = VNAMELEN - 1;
    }

    tmp = g_strdup_printf(_("Enter new name\n(max. %d characters)"),
			  maxlen);
    edit_dialog(0, _("gretl: copy object"), tmp, obj->name, 
		copy_object_callback, obj, 
		VARCLICK_NONE, iconview);
    g_free(tmp);
}

static void rename_object_callback (GtkWidget *widget, dialog_t *dlg) 
{
    gui_obj *obj = (gui_obj *) edit_dialog_get_data(dlg);
    const gchar *newname;
    int err = 0;

    newname = edit_dialog_get_text(dlg);

    if (newname != NULL && *newname != '\0' &&
	strcmp(newname, obj->name)) {
	gchar str[SHOWNAMELEN + 1];

	err = rename_session_object(obj, newname);
	if (!err) {
	    make_short_label_string(str, obj->name);
	    gtk_label_set_text(GTK_LABEL(obj->label), str);
	    mark_session_changed();
	}
    }

    if (!err) {
	edit_dialog_close(dlg);
    }
}

static void rename_object_dialog (gui_obj *obj) 
{
    int maxlen = MAXSAVENAME - 1;
    gchar *tmp;

    if (obj->sort == GRETL_OBJ_MATRIX || obj->sort == GRETL_OBJ_BUNDLE) {
	maxlen = VNAMELEN - 1;
    }

    tmp = g_strdup_printf(_("Enter new name\n(max. %d characters)"),
			  maxlen);
    edit_dialog(0, _("gretl: rename object"), tmp, obj->name, 
		rename_object_callback, obj, 
		VARCLICK_NONE, iconview);
    g_free(tmp);
}

void delete_text_from_session (void *p)
{
    SESSION_TEXT *text = (SESSION_TEXT *) p;
    gui_obj *obj;

    if (text == NULL) return;

    real_delete_text_from_session(text);

    obj = get_gui_obj_by_data((void *) text);
    if (obj != NULL) {
	session_delete_icon(obj);
    }
}

void display_saved_text (void *p)
{
    SESSION_TEXT *text = (SESSION_TEXT *) p;
    PRN *prn;

    if (text == NULL) return;

    prn = gretl_print_new_with_buffer(g_strdup(text->buf));
    if (prn != NULL) { 
	view_buffer(prn, 80, 400, text->name, INFO, NULL);
    }
}

static void session_view_init (void)
{
    iconlist = NULL;
    icon_table = NULL;
    iconview_width = 0;
    iconview_cols = ICONVIEW_MIN_COLS;
}

static void gui_obj_destroy (gui_obj *obj, gpointer p)
{
    if (obj != NULL) {
#if SESSION_DEBUG
	fprintf(stderr, "freeing obj at %p (%s)\n", (void *) obj, obj->name);
#endif
	if (obj->name != NULL) {
	    g_free(obj->name); 
	}
	g_object_unref(obj->icon);
	g_object_unref(obj->label);
	free(obj);
    }
}

static void session_view_free (GtkWidget *w, gpointer data)
{
    iconview = NULL;

    g_list_foreach(iconlist, (GFunc) gui_obj_destroy, NULL);

    g_list_free(iconlist);
    iconlist = NULL;
}

static void session_delete_icon (gui_obj *obj)
{
    if (obj == NULL) return;

    if (obj->icon != NULL && GTK_IS_WIDGET(obj->icon)) {
	gtk_container_remove(GTK_CONTAINER(icon_table), obj->icon);
    }
    if (obj->label != NULL && GTK_IS_WIDGET(obj->label)) {
	gtk_container_remove(GTK_CONTAINER(icon_table), obj->label);
    }

    iconlist = g_list_first(iconlist);
    iconlist = g_list_remove(iconlist, obj);

    gui_obj_destroy(obj, NULL);

    if (iconlist == NULL) {
	fprintf(stderr, "Bad: iconlist has gone NULL\n");
    }
}

/* apparatus for getting a white background */

#if GTK_MAJOR_VERSION >= 3

static void white_bg_style (GtkWidget *widget, gpointer data)
{
    GdkRGBA rgb = {1, 1, 1, 1};

    gtk_widget_override_background_color(widget,
					 GTK_STATE_NORMAL,
					 &rgb);
}

#else

static GdkColor *get_white (void)
{
    GdkColormap *cmap;
    GdkColor *white;

    white = mymalloc(sizeof *white);
    if (white == NULL) return NULL;

    cmap = gdk_colormap_get_system();
    gdk_color_parse("white", white);
    gdk_colormap_alloc_color(cmap, white, FALSE, TRUE);

    return white;
}

static void white_bg_style (GtkWidget *widget, gpointer data)
{
    static GdkColor *white;

    if (white == NULL) {
	white = get_white();
    }

    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, white);
}

#endif

static void real_pack_icon (gui_obj *obj, int row, int col)
{
    obj->row = row;
    obj->col = col;

    gtk_table_attach(GTK_TABLE(icon_table), obj->icon,
		     col, col + 1, row, row + 1,
		     GTK_EXPAND, GTK_FILL, 5, 5);

    gtk_widget_show(obj->icon);
    white_bg_style(obj->icon, NULL);

    gtk_table_attach(GTK_TABLE(icon_table), obj->label,
		     col, col + 1, row + 1, row + 2,
		     GTK_EXPAND, GTK_FILL, 5, 5);

    gtk_widget_show(obj->label);
}

static void pack_single_icon (gui_obj *obj)
{
    int row, col;
    gui_obj *last;

    iconlist = g_list_last(iconlist);
    last = iconlist->data;
    row = last->row;
    col = last->col;  

    iconlist = g_list_append(iconlist, obj);

    col++;
    if (col > 0 && (col % iconview_cols == 0)) {
	col = 0;
	row += 2;
	gtk_table_resize(GTK_TABLE(icon_table), 2 * row, iconview_cols);
    } 

    real_pack_icon(obj, row, col);
}

static void batch_pack_icons (void)
{
    GList *mylist = g_list_first(iconlist);
    gui_obj *obj;
    int row = 0, col = 0;

    while (mylist != NULL) {
	obj = (gui_obj *) mylist->data;
	real_pack_icon(obj, row, col);
	col++;
	if (col > 0 && (col % iconview_cols == 0)) {
	    col = 0;
	    row += 2;
	    gtk_table_resize(GTK_TABLE(icon_table), 2 * row, iconview_cols);
	}
	if (mylist->next == NULL) {
	    break;
	} else {
	    mylist = mylist->next;
	}
    }
}

static int gui_user_var_callback (const char *name, GretlType type,
				  int action)
{
    GretlObjType otype = get_obj_type(type);
    user_var *u;
    int err = 0;

    if (action == UVAR_ADD) {
	/* variable has been added, GUI sync wanted */
	if (iconview != NULL) {
	    u = get_user_var_of_type_by_name(name, type);
	    if (u != NULL) {
		session_add_icon(u, otype, ICON_ADD_SINGLE);
	    }
	} else if (autoicon_on()) {
	    auto_view_session();
	}
	if (iconview != NULL && waiting_for_output()) {
	    gtk_widget_set_sensitive(iconview, FALSE);
	}
	mark_session_changed();
    } else if (action == UVAR_DELETE) {
	/* variable not yet deleted (deferred to GUI) */
	u = get_user_var_of_type_by_name(name, type);
	if (u == NULL) {
	    err = E_UNKVAR;
	} else {
	    maybe_close_window_for_user_var(u, otype);
	    if (iconlist != NULL) {
		session_delete_icon(get_gui_obj_by_data(u));
	    }
	    err = user_var_delete(u);
	}
    }	
    
    return err;
}

void maybe_sensitize_iconview (void)
{
    if (iconview != NULL && !gtk_widget_is_sensitive(iconview)) {
	gtk_widget_set_sensitive(iconview, TRUE);
    }
}

int have_session_objects (void)
{
    int n = data_status;

    /* Note: the following lines enable the icon view even when
       there's no dataset present.
    */

    if (n == 0) {
	n = session.ngraphs + session.ntexts;
    }

    if (n == 0) {
	n = n_user_matrices() + n_user_bundles() + n_user_scalars();
    }

    return n > 0;
}

static void add_user_var_icon (gpointer data, gpointer intp)
{
    session_add_icon(data, GPOINTER_TO_INT(intp), ICON_ADD_BATCH);
}

static void add_all_icons (void) 
{
    int show_graph_page = check_for_program(latex);
    GList *list = NULL;
    int i;

    active_object = NULL;

    if (data_status) {
	session_add_icon(NULL, GRETL_OBJ_INFO,    ICON_ADD_BATCH);  /* data info */
	session_add_icon(NULL, GRETL_OBJ_DSET,    ICON_ADD_BATCH);  /* data file */
	session_add_icon(NULL, GRETL_OBJ_STATS,   ICON_ADD_BATCH);  /* summary stats */
	session_add_icon(NULL, GRETL_OBJ_CORR,    ICON_ADD_BATCH);  /* correlation matrix */
	session_add_icon(NULL, GRETL_OBJ_MODTAB,  ICON_ADD_BATCH);  /* model table */
    }

    /* standard icons that don't really require a dataset in place */
    session_add_icon(NULL, GRETL_OBJ_SCALARS, ICON_ADD_BATCH);  /* scalars */ 
    session_add_icon(NULL, GRETL_OBJ_NOTES,   ICON_ADD_BATCH);  /* session notes */
    if (show_graph_page) {
	session_add_icon(NULL, GRETL_OBJ_GPAGE, ICON_ADD_BATCH); /* graph page */
    }

    for (i=0; i<session.nmodels; i++) {
#if SESSION_DEBUG
	fprintf(stderr, "adding session.models[%d] (type %d) to view\n", i,
		session.models[i]->type);
#endif
	session_add_icon(session.models[i], session.models[i]->type, 
			 ICON_ADD_BATCH);
    }

    for (i=0; i<session.ngraphs; i++) {
#if SESSION_DEBUG
	fprintf(stderr, "adding session.graphs[%d] (type %d) to view\n", i,
		session.graphs[i]->type);
#endif
	session_add_icon(session.graphs[i], session.graphs[i]->type,
			 ICON_ADD_BATCH);
    }

    for (i=0; i<session.ntexts; i++) {
#if SESSION_DEBUG
	fprintf(stderr, "adding session.texts[%d] to view\n", i);
#endif
	session_add_icon(session.texts[i], GRETL_OBJ_TEXT, ICON_ADD_BATCH);
    }

    list = user_var_list_for_type(GRETL_TYPE_MATRIX);
    g_list_foreach(list, add_user_var_icon, GINT_TO_POINTER(GRETL_OBJ_MATRIX));
    g_list_free(list);

    list = user_var_list_for_type(GRETL_TYPE_BUNDLE);
    g_list_foreach(list, add_user_var_icon, GINT_TO_POINTER(GRETL_OBJ_BUNDLE));
    g_list_free(list);

    batch_pack_icons();
}

static void undisplay_icon (gui_obj *obj, gpointer p)
{
    if (obj->icon && GTK_IS_WIDGET(obj->icon))
	gtk_container_remove(GTK_CONTAINER(icon_table), obj->icon);
    if (obj->label && GTK_IS_WIDGET(obj->label))
	gtk_container_remove(GTK_CONTAINER(icon_table), obj->label);
}

static void rearrange_icons (void) 
{
    iconlist = g_list_first(iconlist);
    g_list_foreach(iconlist, (GFunc) undisplay_icon, NULL);
    batch_pack_icons();
}

static gint catch_iconview_key (GtkWidget *w, GdkEventKey *key, 
				gpointer p)
{
#ifdef MAC_NATIVE
    if (key->keyval == GDK_w && cmd_key(key)) {
	gtk_widget_destroy(w);
    }
#endif
    /* 'q' quits iconview */
    if (key->keyval == GDK_q) { 
        gtk_widget_destroy(w);
    }

    return FALSE;
}

static void object_popup_show (gui_obj *obj, GdkEventButton *event)
{
    GtkWidget *w = NULL;

    active_object = obj;

    switch (obj->sort) {
    case GRETL_OBJ_EQN: 
	w = model_popup; 
	break;
    case GRETL_OBJ_MODTAB: 
	w = model_table_popup;
	break;
    case GRETL_OBJ_GPAGE:
	w = graph_page_popup; 
	break;
    case GRETL_OBJ_VAR: 
    case GRETL_OBJ_SYS:
    case GRETL_OBJ_TEXT:
	w = generic_popup; 
	break;
    case GRETL_OBJ_GRAPH: 
    case GRETL_OBJ_PLOT: 
	w = graph_popup; 
	break;
    case GRETL_OBJ_DSET: 
	w = data_popup; 
	break;
    case GRETL_OBJ_SCALARS: 
	w = scalars_popup; 
	break;
    case GRETL_OBJ_INFO: 
	w = info_popup; 
	break;
    case GRETL_OBJ_MATRIX: 
	w = matrix_popup; 
	break;
    case GRETL_OBJ_BUNDLE: 
	w = bundle_popup; 
	break;
    default: 
	break;
    }

    gtk_menu_popup(GTK_MENU(w), NULL, NULL, NULL, NULL,
		   event->button, event->time);
}

static void display_model_table_wrapper (void)
{
    display_model_table(1);
}

static void graph_page_save_wrapper (void)
{
    if (graph_page_get_n_graphs() == 0) {
	warnbox(_("The graph page is empty"));
    } else {
	file_selector(SAVE_TEX, FSEL_DATA_NONE, NULL);
    }
}

static gboolean session_icon_click (GtkWidget *icon, 
				    GdkEventButton *event,
				    gpointer data)
{
    gui_obj *obj = (gui_obj *) data;

    if (event->type == GDK_2BUTTON_PRESS) {
	switch (obj->sort) {
	case GRETL_OBJ_EQN:
	case GRETL_OBJ_VAR:
	case GRETL_OBJ_SYS:
	    display_session_model(obj->data); 
	    break;
	case GRETL_OBJ_GRAPH:
	case GRETL_OBJ_PLOT:
	    open_gui_graph(obj); 
	    break;
	case GRETL_OBJ_TEXT:
	    open_gui_text(obj); 
	    break;
	case GRETL_OBJ_DSET:
	    show_spreadsheet(SHEET_EDIT_DATASET); 
	    break;
	case GRETL_OBJ_SCALARS:
	    edit_scalars();
	    break;
	case GRETL_OBJ_MATRIX:
	    open_matrix(obj); 
	    break;
	case GRETL_OBJ_BUNDLE:
	    open_bundle(obj); 
	    break;
	case GRETL_OBJ_INFO:
	    dataset_info(); 
	    break;
	case GRETL_OBJ_NOTES:
	    edit_session_notes(); 
	    break;
	case GRETL_OBJ_MODTAB:
	    display_model_table_wrapper(); 
	    break;
	case GRETL_OBJ_GPAGE:
	    display_graph_page(iconview); 
	    break;
	case GRETL_OBJ_CORR:
	    do_menu_op(ALL_CORR, NULL, OPT_NONE); 
	    break;
	case GRETL_OBJ_STATS:
	    do_menu_op(ALL_SUMMARY, NULL, OPT_NONE); 
	    break;
	}
	return TRUE;
    } else {
	if (right_click(event)) {
	    if (obj->sort == GRETL_OBJ_EQN  || obj->sort == GRETL_OBJ_GRAPH || 
		obj->sort == GRETL_OBJ_TEXT || obj->sort == GRETL_OBJ_DSET || 
		obj->sort == GRETL_OBJ_INFO || obj->sort == GRETL_OBJ_GPAGE ||
		obj->sort == GRETL_OBJ_PLOT || obj->sort == GRETL_OBJ_MODTAB ||
		obj->sort == GRETL_OBJ_VAR  || obj->sort == GRETL_OBJ_SYS ||
		obj->sort == GRETL_OBJ_MATRIX || obj->sort == GRETL_OBJ_BUNDLE ||
		obj->sort == GRETL_OBJ_SCALARS) {
		object_popup_show(obj, event);
	    }
	    return TRUE;
	}
    }

    return FALSE;
}

static gboolean session_view_click (GtkWidget *widget, 
				    GdkEventButton *event,
				    gpointer data)
{
    if (!in_icon) {
	if (right_click(event)) {
	    /* right-click on iconview background */
	    gtk_menu_popup(GTK_MENU(global_popup), NULL, NULL, NULL, NULL,
			   event->button, event->time);
	    return TRUE;
	}
    }

    return FALSE;
}

static void global_popup_callback (GtkWidget *widget, gpointer data)
{
    gchar *item = (gchar *) data;

    if (!strcmp(item, _("Save session"))) {
	if (sessionfile[0]) {
	    if (session.status & SESSION_CHANGED) {
		save_session(NULL);
	    } else {
		mark_session_saved();
	    }
	} else {
	    file_selector(SAVE_SESSION, FSEL_DATA_NONE, NULL);
	}
    } else if (!strcmp(item, _("Arrange icons"))) {
	rearrange_icons();
    } else if (!strcmp(item, _("Windows"))) {
	window_list_popup(widget, NULL, iconview);
    } else if (!strcmp(item, _("Close window"))) {
	gtk_widget_destroy(iconview);
    }
}

static void info_popup_callback (GtkWidget *widget, gpointer data)
{
    dataset_info();
}

static void matrix_popup_callback (GtkWidget *widget, gpointer data)
{
    gchar *item = (gchar *) data;
    gui_obj *obj = active_object;
    user_var *u = (user_var *) obj->data;
    const char *name = user_var_get_name(u);
    gretl_matrix *m;

    if (!strcmp(item, _("View"))) {
	PRN *prn;

	m = user_var_get_value(u);
	if (m != NULL && bufopen(&prn) == 0) {
	    gretl_matrix_print_to_prn(m, name, prn);
	    view_buffer(prn, 78, 400, name, PRINT, NULL);
	} 
    } else if (!strcmp(item, _("Edit"))) {
	edit_user_matrix_by_name(name, iconview);
    } else if (!strcmp(item, _("Properties"))) {
	m = user_var_get_value(u);
	view_matrix_properties(m, name);
    } else if (!strcmp(item, _("Copy as CSV..."))) {
	m = user_var_get_value(u);
	if (gretl_is_null_matrix(m)) {
	    warnbox("matrix is null");
	} else {
	    matrix_to_clipboard_as_csv(m, iconview);
	}
    } else if (!strcmp(item, _("Rename"))) {
	rename_object_dialog(obj);
    } else if (!strcmp(item, _("Delete"))) {
	maybe_delete_session_object(obj);
    } else if (!strcmp(item, _("Copy"))) {
	copy_object_dialog(obj);
    }
}

static void bundle_popup_callback (GtkWidget *widget, gpointer data)
{
    gchar *item = (gchar *) data;
    gui_obj *obj = active_object;

    if (!strcmp(item, _("View"))) {
	open_bundle(obj);
    } else if (!strcmp(item, _("Rename"))) {
	rename_object_dialog(obj);
    } else if (!strcmp(item, _("Delete"))) {
	maybe_delete_session_object(obj);
    }
}

static void data_popup_callback (GtkWidget *widget, gpointer data)
{
    gchar *item = (gchar *) data;

    if (!strcmp(item, _("Edit"))) {
	show_spreadsheet(SHEET_EDIT_DATASET);
    } else if (!strcmp(item, _("Export as CSV..."))) {
	file_save(mdata, EXPORT_CSV);
    } else if (!strcmp(item, _("Copy as CSV..."))) {
	csv_to_clipboard(iconview);
    }
}

static void scalars_popup_callback (GtkWidget *widget, gpointer data)
{
    gchar *item = (gchar *) data;

    if (!strcmp(item, _("Edit"))) {
	edit_scalars();
    } else if (!strcmp(item, _("Copy as CSV..."))) {
	scalars_to_clipboard_as_csv(iconview);
    }
}

static gchar *object_get_window_title (gui_obj *obj)
{
    gchar *title = NULL;

    if (obj != NULL) {
	title = g_strdup_printf("gretl: %s", obj->name);
    }

    return title;
}

static void object_popup_callback (GtkWidget *widget, gpointer data)
{
    gchar *item = (gchar *) data;
    gui_obj *obj = active_object;

    if (!strcmp(item, _("Display"))) {
	if (obj->sort == GRETL_OBJ_EQN ||
	    obj->sort == GRETL_OBJ_VAR ||
	    obj->sort == GRETL_OBJ_SYS) {
	    display_session_model(obj->data);
	} else if (obj->sort == GRETL_OBJ_TEXT) {
	    open_gui_text(obj);
	} else if (obj->sort == GRETL_OBJ_MODTAB) {
	    display_model_table_wrapper();
	} else if (obj->sort == GRETL_OBJ_GPAGE) {
	    display_graph_page(iconview);
	} else if (obj->sort == GRETL_OBJ_GRAPH || 
		   obj->sort == GRETL_OBJ_PLOT) {
	    open_gui_graph(obj);
	} 
    } else if (!strcmp(item, _("Edit plot commands"))) {
	if (obj->sort == GRETL_OBJ_GRAPH || obj->sort == GRETL_OBJ_PLOT) {
	    SESSION_GRAPH *graph = (SESSION_GRAPH *) obj->data;
	    char fullname[MAXLEN];
	    gchar *title;
	    windata_t *vwin;

	    gretl_chdir(gretl_dotdir());
	    session_file_make_path(fullname, graph->fname);
	    remove_png_term_from_plot_by_name(fullname);
	    title = object_get_window_title(obj);
	    vwin = view_file_with_title(fullname, 1, 0, 78, 400, 
					EDIT_GP, title);
	    g_free(title);
	    /* add flag so we can mark the session as modified
	       if the plot file is changed */
	    vwin->flags |= VWIN_SESSION_GRAPH;
	}
    } else if (!strcmp(item, _("Rename"))) {
	rename_object_dialog(obj);
    } else if (!strcmp(item, _("Delete"))) {
	/* note: "Delete" = "Clear" in some translations */
	if (obj->sort == GRETL_OBJ_MODTAB) {
	    clear_model_table(0, NULL);
	} else if (obj->sort == GRETL_OBJ_GPAGE) {
	    clear_graph_page(0);
	} else {	
	    maybe_delete_session_object(obj);
	}
    } else if (!strcmp(item, _("Add to model table"))) {
	if (obj->sort == GRETL_OBJ_EQN) {
	    SESSION_MODEL *mod = (SESSION_MODEL *) obj->data;

	    add_to_model_table(mod->ptr, MODEL_ADD_FROM_MENU, 0, NULL);
	}
    } else if (!strcmp(item, _("Clear"))) {
	if (obj->sort == GRETL_OBJ_MODTAB) {
	    clear_model_table(0, NULL);
	} else if (obj->sort == GRETL_OBJ_GPAGE) {
	    clear_graph_page(0);
	}
    } else if (!strcmp(item, _("Help"))) {
	if (obj->sort == GRETL_OBJ_MODTAB) {
	    show_gui_help(MODELTAB);
	} else if (obj->sort == GRETL_OBJ_GPAGE) {
	    show_gui_help(GRAPHPG);
	}
    } else if (!strcmp(item, _("Save as TeX..."))) {   
	if (obj->sort == GRETL_OBJ_GPAGE) {
	    graph_page_save_wrapper();
	}
    } else if (!strcmp(item, _("Copy"))) {
	if (obj->sort == GRETL_OBJ_GRAPH || obj->sort == GRETL_OBJ_PLOT) {
	    copy_object_dialog(obj);
	}
    }
}

static gboolean icon_entered (GtkWidget *icon, GdkEventCrossing *event,
			      gui_obj *obj)
{
    gtk_widget_set_state(icon, GTK_STATE_SELECTED);
    in_icon = 1;
    
    return FALSE;
}

static gboolean icon_left (GtkWidget *icon, GdkEventCrossing *event,
			   gui_obj *obj)
{
    gtk_widget_set_state(icon, GTK_STATE_NORMAL);
    in_icon = 0;
    
    return FALSE;
}

static void 
session_data_received (GtkWidget *widget,
		       GdkDragContext *context,
		       gint x,
		       gint y,
		       GtkSelectionData *data,
		       guint info,
		       guint time,
		       gpointer p)
{
    const guchar *seldata = NULL;

    if (data != NULL) {
	seldata = gtk_selection_data_get_data(data);
    }

    if (info == GRETL_MODEL_PTR && seldata != NULL) {
	MODEL **ppmod = (MODEL **) seldata;

	if (ppmod != NULL) {
	    add_to_model_table(*ppmod, MODEL_ADD_BY_DRAG, 0, NULL);
	}
    } else if (info == GRETL_GRAPH_FILE && seldata != NULL) {
	gchar *fname = (gchar *) seldata;

	if (fname != NULL) {
	    graph_page_add_file(fname);
	}
    }
}

static void session_drag_setup (gui_obj *obj)
{
    GtkWidget *w = GTK_WIDGET(obj->icon);
    GtkTargetEntry *targ;
    
    if (obj->sort == GRETL_OBJ_MODTAB) {
	targ = &gretl_drag_targets[GRETL_MODEL_PTR];
    } else {
	targ = &gretl_drag_targets[GRETL_GRAPH_FILE];
    }

    gtk_drag_dest_set (w,
                       GTK_DEST_DEFAULT_ALL,
                       targ, 1,
                       GDK_ACTION_COPY);

    g_signal_connect (G_OBJECT(w), "drag-data-received",
                      G_CALLBACK(session_data_received),
                      NULL);
}

static void drag_graph (GtkWidget *w, GdkDragContext *context,
			GtkSelectionData *sel, guint info, guint t,
			SESSION_GRAPH *graph)
{
    gtk_selection_data_set(sel, GDK_SELECTION_TYPE_STRING, 8, 
                           (const guchar *) graph->fname, 
			   strlen(graph->fname));
}

static void graph_drag_connect (GtkWidget *w, SESSION_GRAPH *graph)
{
    gtk_drag_source_set(w, GDK_BUTTON1_MASK,
                        &gretl_drag_targets[GRETL_GRAPH_FILE],
                        1, GDK_ACTION_COPY);
    g_signal_connect(G_OBJECT(w), "drag-data-get",
                     G_CALLBACK(drag_graph), graph);
}

static void drag_model (GtkWidget *w, GdkDragContext *context,
			GtkSelectionData *sel, guint info, guint t,
			SESSION_MODEL *mod)
{
    gtk_selection_data_set(sel, GDK_SELECTION_TYPE_STRING, 8, 
                           (const guchar *) &mod->ptr, 
			   sizeof mod->ptr);
}

static void model_drag_connect (GtkWidget *w, SESSION_MODEL *mod)
{
    gtk_drag_source_set(w, GDK_BUTTON1_MASK,
                        &gretl_drag_targets[GRETL_MODEL_PTR],
                        1, GDK_ACTION_COPY);
    g_signal_connect(G_OBJECT(w), "drag-data-get",
                     G_CALLBACK(drag_model), mod);
}

#define WANT_TOOLTIP(t) (t == GRETL_OBJ_EQN || \
	                 t == GRETL_OBJ_GRAPH || \
                         t == GRETL_OBJ_PLOT)

static gui_obj *session_add_icon (gpointer data, int sort, int mode)
{
    gui_obj *obj;
    gchar *name = NULL;
    SESSION_GRAPH *graph = NULL;
    SESSION_MODEL *mod = NULL;
    SESSION_TEXT *text = NULL;
    int icon_named = 0;

    switch (sort) {
    case GRETL_OBJ_EQN:
    case GRETL_OBJ_VAR:
    case GRETL_OBJ_SYS:
	mod = (SESSION_MODEL *) data;
	name = g_strdup(mod->name);
	break;
    case GRETL_OBJ_PLOT:
    case GRETL_OBJ_GRAPH:
	graph = (SESSION_GRAPH *) data;
	name = g_strdup(graph->name);
	break;
    case GRETL_OBJ_TEXT:
	text = (SESSION_TEXT *) data;
	name = g_strdup(text->name);
	break;
    case GRETL_OBJ_DSET:
	name = g_strdup(_("Data set"));
	break;
    case GRETL_OBJ_SCALARS:
	name = g_strdup(_("Scalars"));
	break;
    case GRETL_OBJ_INFO:
	name = g_strdup(_("Data info"));
	break;
    case GRETL_OBJ_NOTES:
	name = g_strdup(_("Notes"));
	break;
    case GRETL_OBJ_CORR:
	name = g_strdup(_("Correlations"));
	break;
    case GRETL_OBJ_STATS:
	name = g_strdup(_("Summary"));
	break;
    case GRETL_OBJ_MODTAB:
	name = g_strdup(_("Model table"));
	break;
    case GRETL_OBJ_GPAGE:
	name = g_strdup(_("Graph page"));
	break;
    case GRETL_OBJ_MATRIX:
    case GRETL_OBJ_BUNDLE:
	name = g_strdup(user_var_get_name((user_var *) data));
	break;
    default:
	break;
    }

    if (name == NULL) {
	fprintf(stderr, "session_add_icon: got NULL name\n");
	return NULL;
    }

    obj = gui_object_new(name, sort, data);

    /* full-length object name as tooltip */
    if (strlen(name) > SHOWNAMELEN) {
	gretl_tooltips_add(GTK_WIDGET(obj->icon), name);
	icon_named = 1;
    }

    /* set up for drag and drop */
    if (sort == GRETL_OBJ_EQN) {
	model_drag_connect(obj->icon, obj->data);
    } else if (sort == GRETL_OBJ_GRAPH) {
	graph_drag_connect(obj->icon, obj->data);
    }

    /* second try at adding a tooltip */
    if (WANT_TOOLTIP(sort) && !icon_named) {
	char *str = NULL;
	    
	if (sort == GRETL_OBJ_EQN) {
	    MODEL *pmod = (MODEL *) mod->ptr;

	    str = model_cmd_str(pmod);
	} else if (sort == GRETL_OBJ_GRAPH ||
		   sort == GRETL_OBJ_PLOT) {
	    str = graph_str(graph);
	} 
	if (str != NULL) {
	    gretl_tooltips_add(GTK_WIDGET(obj->icon), str);
	    free(str);
	}
    }

    if (sort == GRETL_OBJ_DSET) {
	obj->data = datafile;
    }

    if (mode == ICON_ADD_SINGLE) {
	pack_single_icon(obj);
    } else if (mode == ICON_ADD_BATCH) {
	iconlist = g_list_append(iconlist, obj);
    }

    return obj;
}

static GtkWidget *create_pop_item (GtkWidget *popup, char *str, 
				   GtkCallback callback)
{
    GtkWidget *item;

    item = gtk_menu_item_new_with_label(str);
    g_signal_connect(G_OBJECT(item), "activate",
		     G_CALLBACK(callback),
		     str);
    gtk_widget_show(item);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    if (!strcmp(str, _("Save session"))) {
	save_item = item;
	if (session.status == 0) {
	    gtk_widget_set_sensitive(item, FALSE);
	}
    }

    return item;
}

static void session_build_popups (void)
{
    size_t i, n;

    if (global_popup == NULL) {
	global_popup = gtk_menu_new();
	n = G_N_ELEMENTS(global_items);
	for (i=0; i<n; i++) {
	    create_pop_item(global_popup, _(global_items[i]), 
			    global_popup_callback);
	}
    }

    if (model_popup == NULL) {
	model_popup = gtk_menu_new();
	n = G_N_ELEMENTS(model_items);
	for (i=0; i<n; i++) {
	    create_pop_item(model_popup, _(model_items[i]), 
			    object_popup_callback);
	}
    }

    if (model_table_popup == NULL) {
	model_table_popup = gtk_menu_new();
	n = G_N_ELEMENTS(model_table_items);
	for (i=0; i<n; i++) {
	    create_pop_item(model_table_popup, _(model_table_items[i]), 
			    object_popup_callback);
	}
    }

    if (generic_popup == NULL) {
	generic_popup = gtk_menu_new();
	n = G_N_ELEMENTS(generic_items);
	for (i=0; i<n; i++) {
	    create_pop_item(generic_popup, _(generic_items[i]), 
			    object_popup_callback);
	}
    }

    if (graph_popup == NULL) {
	graph_popup = gtk_menu_new();
	n = G_N_ELEMENTS(graph_items);
	for (i=0; i<n; i++) {
	    create_pop_item(graph_popup, _(graph_items[i]), 
			    object_popup_callback);
	}
    }

    if (graph_page_popup == NULL) {
	graph_page_popup = gtk_menu_new();
	n = G_N_ELEMENTS(graph_page_items);
	for (i=0; i<n; i++) {
	    create_pop_item(graph_page_popup, _(graph_page_items[i]), 
			    object_popup_callback);
	}
    }

    if (data_popup == NULL) {
	data_popup = gtk_menu_new();
	n = G_N_ELEMENTS(dataset_items);
	for (i=0; i<n; i++) {
	    create_pop_item(data_popup, _(dataset_items[i]), 
			    data_popup_callback);	    
	}
    }

    if (scalars_popup == NULL) {
	scalars_popup = gtk_menu_new();
	n = G_N_ELEMENTS(scalars_items);
	for (i=0; i<n; i++) {
	    create_pop_item(scalars_popup, _(scalars_items[i]), 
			    scalars_popup_callback);	    
	}
    }

    if (info_popup == NULL) {
	info_popup = gtk_menu_new();
	n = G_N_ELEMENTS(info_items);
	for (i=0; i<n; i++) {
	    create_pop_item(info_popup, _(info_items[i]), 
			    info_popup_callback);	    
	}
    }

    if (matrix_popup == NULL) {
	matrix_popup = gtk_menu_new();
	n = G_N_ELEMENTS(matrix_items);
	for (i=0; i<n; i++) {
	    create_pop_item(matrix_popup, _(matrix_items[i]), 
			    matrix_popup_callback);	    
	}
    }

    if (bundle_popup == NULL) {
	bundle_popup = gtk_menu_new();
	n = G_N_ELEMENTS(bundle_items);
	for (i=0; i<n; i++) {
	    create_pop_item(bundle_popup, _(bundle_items[i]), 
			    bundle_popup_callback);	    
	}
    }
}

static gboolean 
iconview_resize_callback (GtkWidget *w, GdkEventConfigure *e, gpointer p)
{
    if (e->width != iconview_width) {
	if (iconview_width > 0) {
	    int cols = e->width / 100;

	    if (cols >= ICONVIEW_MIN_COLS && cols != iconview_cols) {
		iconview_cols = cols;
		rearrange_icons();
	    }
	} 
	iconview_width = e->width;
    }

    return FALSE;
}

void view_session (void)
{
    GtkWidget *ebox, *scroller;
    gchar *title;

    if (iconview != NULL) {
	gtk_window_present(GTK_WINDOW(iconview));
	return;
    }

    session_view_init();

    iconview = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    title = g_strdup_printf("gretl: %s", _("icon view"));
    gtk_window_set_title(GTK_WINDOW(iconview), title);
    g_free(title);
    gtk_window_set_default_size(GTK_WINDOW(iconview), 400, 300);

    gtk_container_set_border_width(GTK_CONTAINER(iconview), 0);
    g_signal_connect(G_OBJECT(iconview), "destroy",
		     G_CALLBACK(session_view_free), NULL);

    session_build_popups();

    ebox = gtk_event_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(ebox), 5);
    gtk_container_add(GTK_CONTAINER(iconview), ebox);
    g_signal_connect(G_OBJECT(ebox), "button-press-event",
		     G_CALLBACK(session_view_click), NULL);

    scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scroller), 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(ebox), scroller);

    icon_table = gtk_table_new(2, ICONVIEW_MIN_COLS, FALSE);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroller), 
					  icon_table);

    add_all_icons();
    window_list_add(iconview, OPEN_SESSION);
    g_signal_connect(G_OBJECT(iconview), "key-press-event",
		     G_CALLBACK(catch_iconview_key), NULL);
    g_signal_connect(G_OBJECT(iconview), "configure-event",
		     G_CALLBACK(iconview_resize_callback), NULL);

    gtk_widget_show_all(iconview);

    gtk_container_foreach(GTK_CONTAINER(scroller), 
			  (GtkCallback) white_bg_style, 
			  NULL);

    gtk_widget_set_can_focus(icon_table, TRUE);
    gtk_widget_grab_focus(icon_table);
}

static int view_session_deferred;

static void auto_view_session (void)
{
    if (output_flush_in_progress()) {
	view_session_deferred = 1;
    } else {
	view_session();
    }
}

void maybe_view_session (void)
{
    if (view_session_deferred) {
	view_session_deferred = 0;
	view_session();
    }
}

static void make_short_label_string (char *targ, const char *src)
{
    if (g_utf8_strlen(src, -1) > SHOWNAMELEN) {
	g_utf8_strncpy(targ, src, SHOWNAMELEN - 3);
	strcat(targ, "...");
    } else {
	strcpy(targ, src);
    }
}

static void create_gobj_icon (gui_obj *obj, const char **xpm)
{
    gchar str[2*SHOWNAMELEN];
    GdkPixbuf *pbuf;
    GtkWidget *image;

    pbuf = gdk_pixbuf_new_from_xpm_data(xpm);

    obj->icon = gtk_event_box_new();
    gtk_widget_set_size_request(obj->icon, 36, 36);

    image = gtk_image_new_from_pixbuf(pbuf);
    g_object_unref(G_OBJECT(pbuf));

    gtk_container_add(GTK_CONTAINER(obj->icon), image);
    gtk_widget_show(image);

    if (obj->sort == GRETL_OBJ_MODTAB || obj->sort == GRETL_OBJ_GPAGE) {
	session_drag_setup(obj);
    }

    make_short_label_string(str, obj->name);
    obj->label = gtk_label_new(str);

    g_object_ref(obj->icon);
    g_object_ref(obj->label);

    g_signal_connect(G_OBJECT(obj->icon), "button-press-event",
		     G_CALLBACK(session_icon_click), obj);
    g_signal_connect(G_OBJECT(obj->icon), "enter-notify-event",
		     G_CALLBACK(icon_entered), obj);
    g_signal_connect(G_OBJECT(obj->icon), "leave-notify-event",
		     G_CALLBACK(icon_left), obj);
}

static gui_obj *gui_object_new (gchar *name, int sort, gpointer data)
{
    gui_obj *obj;
    char **xpm = NULL;

    obj = mymalloc(sizeof *obj);
    if (obj == NULL) {
	return NULL;
    }

    obj->name = name; 
    obj->sort = sort;
    obj->data = data;

#if SESSION_DEBUG
    fprintf(stderr, "Allocated obj at %p (%s)\n", (void *) obj, obj->name);
#endif

    switch (sort) {
    case GRETL_OBJ_EQN:
    case GRETL_OBJ_VAR:
    case GRETL_OBJ_SYS:     xpm = model_xpm;       break;
    case GRETL_OBJ_PLOT:    xpm = boxplot_xpm;     break;
    case GRETL_OBJ_GRAPH:   xpm = gnuplot_xpm;     break;
    case GRETL_OBJ_DSET:    xpm = dot_sc_xpm;      break;
    case GRETL_OBJ_SCALARS: xpm = dot_sc_xpm;      break;
    case GRETL_OBJ_INFO:    xpm = xfm_info_xpm;    break;
    case GRETL_OBJ_TEXT:
    case GRETL_OBJ_NOTES:   xpm = text_xpm;        break;
    case GRETL_OBJ_CORR:    xpm = rhohat_xpm;      break;
    case GRETL_OBJ_STATS:   xpm = summary_xpm;     break;
    case GRETL_OBJ_MODTAB:  xpm = model_table_xpm; break;
    case GRETL_OBJ_GPAGE:   xpm = graph_page_xpm;  break;
    case GRETL_OBJ_MATRIX:  xpm = matrix_xpm;      break;
    case GRETL_OBJ_BUNDLE:  xpm = bundle_xpm;      break;
    default: break;
    }

    create_gobj_icon(obj, (const char **) xpm);

    return obj;
} 

static void real_open_session_graph (SESSION_GRAPH *graph)
{
    char tmp[MAXLEN];

    session_file_make_path(tmp, graph->fname);
    display_session_graph(tmp, graph->name);
}

static void open_gui_graph (gui_obj *obj)
{
    real_open_session_graph((SESSION_GRAPH *) obj->data);
}

void display_session_graph_by_data (void *p)
{
    real_open_session_graph((SESSION_GRAPH *) p);
}

static int is_idempotent (const gretl_matrix *m,
			  const gretl_matrix *evals)
{
    if (evals != NULL) {
	int i;

	for (i=0; i<m->rows; i++) {
	    if (evals->val[i] != 0.0 && evals->val[i] != 1.0) {
		return 0;
	    }
	}
    }

    return gretl_matrix_is_idempotent(m);
}

static void print_int_formatted (char *s, int k, PRN *prn)
{
    int len = 12, n = strlen(s) - g_utf8_strlen(s, -1);
    char fmt[16];

    if (n > 0) {
	len += n;
    }

    sprintf(fmt, "%%-%ds %%3d\n", len);
    pprintf(prn, fmt, s, k);
}

static void print_double_formatted (char *s, double x, PRN *prn)
{
    int len = 16, n = strlen(s) - g_utf8_strlen(s, -1);
    char fmt[16];

    if (n > 0) {
	len += n;
    }

    sprintf(fmt, "%%-%ds %%.8g\n", len);    
    pprintf(prn, fmt, s, x);    
}

void 
view_matrix_properties (const gretl_matrix *m, const char *name)
{
    gretl_matrix *A = NULL;
    gretl_matrix *evals = NULL;
    gchar *title;
    PRN *prn;
    int s, err = 0;

    if (m == NULL || bufopen(&prn)) {
	return;
    }

    pprintf(prn, _("Properties of matrix %s"), (name != NULL)? name : "");
    pputs(prn, "\n\n");

    if (m->rows == 0 || m->cols == 0) {
	pprintf(prn, _("Null matrix, %d x %d\n"), m->rows, m->cols);
	goto done;
    } else if (m->rows == 1 && m->cols == 1) {
	pprintf(prn, _("Scalar matrix, value %g\n"), m->val[0]);
	goto done;
    } else if (gretl_is_identity_matrix(m)) {
	pprintf(prn, _("Identity matrix, order %d\n"), m->rows);
	goto done;
    } else if (gretl_is_zero_matrix(m)) {
	pprintf(prn, _("Null matrix, %d x %d\n"), m->rows, m->cols);
	goto done;
    } 

    print_int_formatted(_("Rows"), m->rows, prn);
    print_int_formatted(_("Columns"), m->cols, prn);
    print_int_formatted(_("Rank"), gretl_matrix_rank(m, &err), prn);

    s = gretl_matrix_get_structure(m);

    if (s > 0) {
	pprintf(prn, "%s\n", _("Square"));
    }

    if (s == GRETL_MATRIX_DIAGONAL) {
	pprintf(prn, "%s\n", _("Diagonal"));
    } else if (s == GRETL_MATRIX_LOWER_TRIANGULAR) {
	pprintf(prn, "%s\n", _("Lower triangular"));
    } else if (s == GRETL_MATRIX_UPPER_TRIANGULAR) {
	pprintf(prn, "%s\n", _("Upper triangular"));
    } else if (s == GRETL_MATRIX_SYMMETRIC) {
	pprintf(prn, "%s\n", _("Symmetric"));
	A = gretl_matrix_copy(m);
	if (A != NULL) {
	    err = gretl_matrix_cholesky_decomp(A);
	    if (!err) {
		pprintf(prn, "%s\n", _("Positive definite"));
	    } else {
		pprintf(prn, "%s\n", _("Not positive definite"));
	    }
	    gretl_matrix_copy_values(A, m);
	    err = 0;
	    evals = gretl_symmetric_matrix_eigenvals(A, 0, &err);
	}
    } 

    if (s > 0 && (s != GRETL_MATRIX_SYMMETRIC || evals == NULL)) {
	gretl_matrix_free(A);
	A = gretl_matrix_copy(m);
	if (A != NULL) {
	    err = 0;
	    evals = gretl_general_matrix_eigenvals(A, 0, &err);
	}
    }

    if (s > 0) {
	if (is_idempotent(m, evals)) {
	    pprintf(prn, "%s\n", _("Idempotent"));
	} else {
	    pprintf(prn, "%s\n", _("Not idempotent"));
	}
    }

    pputc(prn, '\n');

    print_double_formatted(_("1-norm"), gretl_matrix_one_norm(m), prn);
    print_double_formatted(_("Infinity-norm"), gretl_matrix_infinity_norm(m), prn);
	    
    if (m->rows == m->cols) {
	double det;

	print_double_formatted(_("Trace"), gretl_matrix_trace(m), prn);
	if (A == NULL) {
	    A = gretl_matrix_copy(m);
	} else {
	    gretl_matrix_copy_values(A, m);
	}
	if (A != NULL) {
	    det = gretl_matrix_determinant(A, &err);
	    if (!err) {
		print_double_formatted(_("Determinant"), det, prn);
	    }
	}
    }

    if (evals != NULL) {
	int i;

	pprintf(prn, "\n%s:\n", _("Eigenvalues"));

	for (i=0; i<m->rows; i++) {
	    if (s != GRETL_MATRIX_SYMMETRIC) {
		pprintf(prn, "  (%.8g, %.8g)\n", gretl_matrix_get(evals, i, 0),
			gretl_matrix_get(evals, i, 1));
	    } else {
		pprintf(prn, "  %.8g\n", evals->val[i]);
	    }
	}

	gretl_matrix_free(evals);
    }

    if (A != NULL) {
	gretl_matrix_free(A);
    }

 done:

    title = gretl_window_title(name);
    view_buffer(prn, 78, 400, title, PRINT, NULL);
    g_free(title);
}
