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

#ifndef LIBGLUE_H
#define LIBGLUE_H

int model_test_driver (const char *param, DATASET *dset, 
		       gretlopt opt, PRN *prn);

int chow_test_driver (const char *line, MODEL *pmod, 
		      DATASET *dset, gretlopt opt, PRN *prn);

int llc_test_driver (const char *parm, const int *list, 
		     DATASET *dset, gretlopt opt, PRN *prn);

MODEL quantreg_driver (const char *parm, const int *list, 
		       DATASET *dset, gretlopt opt, PRN *prn);

MODEL logit_probit (int *list, DATASET *dset, 
		    int ci, gretlopt opt, PRN *prn);

MODEL logistic_driver (const int *list, DATASET *dset,
		       gretlopt opt); 

MODEL tobit_driver (const int *list, DATASET *dset, 
		    gretlopt opt, PRN *prn);

int matrix_command_driver (int ci, 
			   const int *list, 
			   const char *param,
			   const DATASET *dset, 
			   gretlopt opt,
			   PRN *prn);

int matrix_freq_driver (const int *list,
			int *graph,
			gretlopt opt,
			PRN *prn);

int list_summary_driver (const int *list, 
			 const DATASET *dset, 
			 gretlopt opt,
			 PRN *prn);

int do_modprint (const char *line, gretlopt opt, PRN *prn);

#endif /* LIBGLUE_H */
