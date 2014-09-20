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

/* gretl audio graph plugin */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gretl/lib/libgretl.h>
#include <gretl/lib/version.h>
#include <gretl/config.h>

#include <gretl/plugin/miditypes.h>
#include <gretl/plugin/midi_utils.h>

#include <glib.h>
#include <gtk/gtk.h>

#define ADEBUG 0

#ifdef HAVE_FLITE
 #include <flite/flite.h>
 extern cst_voice *register_cmu_us_kal (void);
#endif

#ifdef WIN32_SAPI
 #define COBJMACROS
 #include <sapi.h>
#endif

#if defined(HAVE_FLITE) || defined(WIN32_SAPI)
 #define DO_SPEECH
#endif

const char *track_hdr;

enum dataset_comments {
    TS_COMMENT = 0,
    YLABEL_COMMENT,
    XLABEL_COMMENT,
    XRANGE_COMMENT,
    N_COMMENTS
};

#define DEFAULT_FORCE 96 /* volume of MIDI notes */

#ifndef G_OS_WIN32
#define DEFAULT_MIDI_PROG "timidity" /* MIDI player to be used */
#define DEFAULT_MIDI_OPT  "-A500"    /* option to pass to MIDI player */
#endif

typedef struct _datapoint datapoint;
typedef struct _dataset dataset;
typedef struct _midi_spec midi_spec;
typedef struct _midi_track midi_track;
typedef struct _note_event note_event;

struct _datapoint {
    double x;
    double y;
};

struct _dataset {
    int pd;
    int n;
    int series2;
    double intercept;
    double slope;
    datapoint *points;
    double *y2;
    gchar *comments[N_COMMENTS];
};

struct _midi_spec {
    int ntracks;
    int nticks;
    int nsecs;
    dataset *dset;
    FILE *fp;
};

struct _midi_track {
    unsigned char channel;
    unsigned char patch;
    int n_notes;
    note_event *notes;
};

struct _note_event {
    double dtime;
    double duration;
    unsigned char pitch;
    unsigned char force;
    unsigned char channel;
};

#define delta_time_zero(f) (putc(0x00, f))

#define YMAX 60

static int write_note_event (note_event *event, midi_spec *spec);

static void write_midi_track (midi_track *track,
                  midi_spec *spec);

static void make_ts_comment (const char *line, dataset *dset);

static void points_min_max (const datapoint *points,
                double *xmin, double *xmax,
                double *ymin, double *ymax,
                int n);

static void min_max (const double *x, double *min, double *max,
             int n);

#if defined(HAVE_FLITE)

#if 0
static int save_dataset_comments (const dataset *dset);
#endif

static void speak_dataset_comments (const dataset *dset);

#elif defined(WIN32_SAPI)

static WCHAR *wide_string (const char *s);

static void speak_dataset_comments (const dataset *dset);

#endif

static void print_dataset_comments (const dataset *dset);

static void audio_graph_error (const char *msg);

static int compare_points (const void *a, const void *b);

static int play_dataset (midi_spec *spec, midi_track *track,
             const dataset *dset);

#if defined(G_OS_WIN32)

static int midi_fork (const char *fname, const char *midiplayer);

#else /* non-Windows version */

static char **get_midi_args (int *argc, const char *fname,
                 const char *midiplayer);

static int real_midi_fork (char **argv);

static int midi_fork (const char *fname, const char *midiplayer);

#endif /* ! MS Windows */

int midi_play_graph (const char *fname, const char *userdir,
             const char *midiplayer);

#endif // AUDIO_H
