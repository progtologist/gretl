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

#ifndef PRINTSCAN_H
#define PRINTSCAN_H

#include <gretl/lib/libgretl.h>

int do_printf (const char *targ, const char *format, const char *args,
	       DATASET *dset, PRN *prn, int *nchars);

int do_sscanf (const char *src, const char *format, const char *args,
	       DATASET *dset, int *n_items);

int do_printscan_command (const char *line, DATASET *dset, PRN *prn);

int generate_obs_markers (const char *s, DATASET *dset);

#endif /* PRINTSCAN_H */
