/* print some window contents to a buffer suitable for
   text-to-speech
*/

#ifndef AUDIOPRINT_H
#define AUDIOPRINT_H

#include <gretl/gui2/gretl_enums.h>
#include <gretl/config.h>
#include <gretl/plugin/audio.h>

#ifdef HAVE_FLITE
 #include <flite/flite.h>
 extern cst_voice *register_cmu_us_kal (void);
#endif

#ifdef WIN32_SAPI
 #define COBJMACROS
 #include <sapi.h>
#endif

static int audioprint_coeff (const DATASET *dset, const MODEL *pmod,
                  int i, PRN *prn);

static int audioprint_coefficients (const MODEL *pmod, const DATASET *dset,
                    PRN *prn);

static void audio_rsqline (const MODEL *pmod, PRN *prn);

static void
audioprint_model (MODEL *pmod, const DATASET *dset, PRN *prn);

static void
audioprint_summary (Summary *summ, const DATASET *dset,
            PRN *prn);

static void
audioprint_matrix (const VMatrix *vmat, const DATASET *dset,
           PRN *prn);

static void
audioprint_coeff_interval (const CoeffIntervals *cf, int i, PRN *prn);

static void
audioprint_confints (const CoeffIntervals *cf, PRN *prn);

#ifdef HAVE_FLITE

static int speak_buffer (const char *buf, int (*should_stop)());

static int speak_line (const char *line);

#else

static ISpVoice *get_sapi_voice (void);

static void release_sapi_voice (ISpVoice *v);

static int speak_buffer (const char *buf, int (*should_stop)());

static int speak_line (const char *line);

#endif

static int audio_print_special (int role, void *data, const DATASET *dset,
                int (*should_stop)());

static int read_listbox_content (GtkWidget *listbox, int (*should_stop)());

int read_window_text (GtkWidget *listbox,
              GtkWidget *text,
              int role,
              gpointer data,
              const DATASET *dset,
              int (*should_stop)());

#endif // AUDIOPRINT_H
