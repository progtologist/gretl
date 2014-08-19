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

#ifndef IMPORT_COMMON_H
#define IMPORT_COMMON_H

#if (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 18)
 #include <gtk/gtk_compat.h>
#endif

#if GTK_MAJOR_VERSION >= 3
 #include <gdk/gdkkeysyms-compat.h>
#else
 #include <gdk/gdkkeysyms.h>
#endif

#include <gretl/lib/libgretl.h>
#include <gretl/plugin/importer.h>
#include <gtk/gtk.h>

int check_imported_varname (char *vname, int row, int col,
                   PRN *prn);

#ifndef EXCEL_IMPORTER /* FIXME? */

void import_ts_check (DATASET *dset);

#endif /* !EXCEL_IMPORTER */

#if defined(ODS_IMPORTER) || defined(XLSX_IMPORTER)

char *get_absolute_path (const char *fname);

void remove_temp_dir (char *dname);

#ifdef G_OS_WIN32

int gretl_make_tempdir (char *dname);

#else

int gretl_make_tempdir (char *dname);

# endif /* G_OS_WIN32 or not */

/* For ODS and XLSX: unzip the target file in the user's
   "dotdir". On successful completion @dname holds the
   name of the temporary subdirectory, in the dotdir,
   holding the contents of the zipfile.
*/

int open_import_zipfile (const char *fname, char *dname,
                PRN *prn);

/* check for spurious empty columns at the right of the sheet */
int import_prune_columns (DATASET *dset);

#else /* !ODS, !XLSX */

int worksheet_start_dataset (DATASET *newinfo);

int
importer_dates_check (char **labels, BookFlag *pflags,
              DATASET *newset, PRN *prn,
              int *err);

void wbook_print_info (wbook *book);

void wbook_free (wbook *book);

int wbook_check_params (wbook *book);

void wbook_record_params (wbook *book, int *list);

#endif /* !ODS_IMPORTER */

/* @list may contain sheet number, row and/or column offset;
   @sheetname may contain the name of a specific sheet; but
   both may be NULL
*/

void wbook_init (wbook *book, const int *list, char *sheetname);

const char *column_label (int col);

void colspin_changed (GtkEditable *ed, GtkWidget *w);

#ifdef EXCEL_IMPORTER
 #ifndef WIN32

void infobox (const char *template, ...);

 #endif /* !WIN32 */

static
void debug_callback (GtkWidget *w, wbook *book);

#endif /* EXCEL_IMPORTER */

int book_get_min_offset (wbook *book, int k);

static
void wsheet_menu_select_row (GtkTreeSelection *selection, wbook *book);

static
void wsheet_menu_make_list (GtkTreeView *view, wbook *book);

static
void wsheet_menu_cancel (GtkWidget *w, wbook *book);

static
void wbook_set_col_offset (GtkWidget *w, wbook *book);

static
void wbook_set_row_offset (GtkWidget *w, wbook *book);

static
void add_sheets_list (GtkWidget *vbox, wbook *book);

void make_wmenu_modal (GtkWidget *w, gpointer p);

gboolean esc_cancels (GtkWidget *w, GdkEventKey *key, wbook *book);

void wsheet_menu (wbook *book, int multisheet);

#endif // IMPORT_COMMON_H
