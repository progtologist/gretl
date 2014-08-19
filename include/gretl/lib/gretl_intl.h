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

#ifndef GRETL_INTL_H
#define GRETL_INTL_H

void gretl_push_c_numeric_locale (void);

void gretl_pop_c_numeric_locale (void);

int doing_nls (void);

int reset_local_decpoint (void);

int get_local_decpoint (void);

char *iso_to_ascii (char *s);

/* the following enumeration is organized by alphabetical order of
   English name of language: Albanian, Basque, Bulgarian, ...
*/

typedef enum {
    LANG_AUTO = 0,
    LANG_C,
    LANG_SQ,
    LANG_EU,
    LANG_BG,
    LANG_CA,
    LANG_ZH_TW,
    LANG_CS,
    LANG_FR,
    LANG_GL,
    LANG_DE,
    LANG_EL,
    LANG_IT,
    LANG_PL,
    LANG_PT,
    LANG_PT_BR,
    LANG_RU,
    LANG_ES,
    LANG_TR,
    LANG_MAX
} GretlLangCode;

const char *lang_string_from_id (int langid);

char *utf8_to_cp (const char *s);

char *utf8_to_latin (const char *s);

int iso_latin_version (void);

int lang_id_from_name (const char *s);

int lang_id_from_code (const char *s);

void set_alt_gettext_mode (PRN *prn);

void set_lcnumeric (int langid, int lcnumeric);

int gretl_is_ascii (const char *buf);

int force_language (int langid);

int test_locale (const char *langstr);

#ifdef ENABLE_NLS

char *iso_gettext (const char *msgid);

char *alt_gettext (const char *msgid);

void set_gretl_charset (void);

void get_suitable_tex_encoding (char *encfile);

char *sprint_l2_to_ascii (char *targ, const char *s, size_t len);

int get_utf_width (const char *str, int width);

int get_translated_width (const char *str);

int chinese_locale (void);

# define UTF_WIDTH(s, w) get_utf_width(s, w) 
# define TRANSLATED_WIDTH(s) get_translated_width(s)

#else /* !ENABLE_NLS */

#define gettext(s) s
#define iso_gettext(s) s
#define alt_gettext(s) s

# define UTF_WIDTH(s, w) w
# define TRANSLATED_WIDTH(s) strlen(s)

#endif /* ENABLE_NLS */

#endif /* GRETL_INTL_H */
