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

#ifndef GRETL_ZIP_H
#define GRETL_ZIP_H

int gretl_unzip_file (const char *fname, GError **gerr);

int gretl_make_zipfile (const char *fname, const char *path,
			GError **gerr);

int gretl_unzip_session_file (const char *fname, gchar **zdirname, 
			      GError **gerr);

int gretl_zip_datafile (const char *fname, const char *path,
			int level, GError **gerr);

int gretl_unzip_datafile (const char *fname, const char *path,
			  GError **gerr);

#endif /* GRETL_ZIP_H */
