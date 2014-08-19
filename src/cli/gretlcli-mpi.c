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

/* command-line client program for libgretl, mpi version */

#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>
#include <mpi/mpi.h>

#include <gretl/lib/libgretl.h>
#include <gretl/lib/version.h>
#include <gretl/lib/monte_carlo.h>
#include <gretl/lib/var.h>
#include <gretl/lib/system.h>
#include <gretl/lib/gretl_restrict.h>
#include <gretl/lib/gretl_func.h>
#include <gretl/lib/libset.h>
#include <gretl/lib/cmd_private.h>
#include <gretl/lib/flow_control.h>
#include <gretl/lib/objstack.h>
#include <gretl/lib/gretl_xml.h>
#include <gretl/lib/gretl_string_table.h>
#include <gretl/lib/dbread.h>
#include <gretl/lib/uservar.h>
#include <gretl/lib/csvdata.h>
#ifdef USE_CURL
 #include <gretl/lib/gretl_www.h>
#endif

#ifdef WIN32
# include <windows.h>
#else
# include <sys/stat.h>
# include <sys/types.h>
# include <fcntl.h>
# include <unistd.h>
#endif 

char datafile[MAXLEN];
char cmdfile[MAXLEN];
FILE *fb;
int runit;
int data_status;
char linebak[MAXLINE];

static int cli_exec_line (ExecState *s, int id, DATASET *dset, 
			  gretlopt progopt, PRN *cmdprn);
static int push_input_file (FILE *fp);
static FILE *pop_input_file (void);
static int cli_saved_object_action (const char *line, 
				    DATASET *dset, 
				    PRN *prn);

static int parse_options (int *pargc, char ***pargv, gretlopt *popt, 
			  double *scriptval, int *dcmt, char *fname)
{
    char **argv;
    int argc, gotfile = 0;
    gretlopt opt = OPT_NONE;
    int err = 0;

    *fname = '\0';

    if (pargv == NULL) {
	return E_DATA;
    }

    argc = *pargc;
    argv = *pargv;

    while (*++argv) {
	const char *s = *argv;

	if (!strcmp(s, "-e") || !strncmp(s, "--english", 9)) { 
	    opt |= OPT_ENGLISH;
	} else if (!strcmp(s, "-h") || !strcmp(s, "--help")) {
	    opt |= OPT_HELP;
	} else if (!strcmp(s, "-v") || !strcmp(s, "--version")) {
	    opt |= OPT_VERSION;
	} else if (!strcmp(s, "-q") || !strcmp(s, "--quiet")) {
	    opt |= OPT_QUIET;
	} else if (!strcmp(s, "-s") || !strcmp(s, "--single-rng")) { 
	    *dcmt = 0;
	} else if (!strncmp(s, "--scriptopt=", 12)) {
	    *scriptval = atof(s + 12);
	} else if (*s == '-') {
	    /* not a valid option */
	    err = E_DATA;
	    break;
	} else if (!gotfile) {
	    strncat(fname, s, MAXLEN - 1);
	    gotfile = 1;
	}
	argc--;
    }

    if (!(opt & (OPT_HELP|OPT_VERSION)) && !gotfile) {
	err = E_DATA;
    }

    *pargc = argc;
    *pargv = argv;
    *popt = opt;

    return err;
}

static void mpi_exit (int err)
{
    if (err) {
	MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    } else {
	MPI_Finalize();
	exit(EXIT_SUCCESS);
    }
}

static void usage (int err)
{
    printf("gretlcli-mpi %s\n", GRETL_VERSION);
    printf(_("This program should be run under mpiexec, and requires "
	     "the name of a\nscript file as argument.\n"));
    printf(_("Options:\n"
	     " -h or --help        Print this info and exit.\n"
	     " -v or --version     Print version info and exit.\n"
             " -q or --quiet       Don't print logo on start-up.\n"
	     " -e or --english     Force use of English rather than translation.\n"
	     " -s or --single-rng  Use a single RNG, not one per process.\n"));
    printf(" --scriptopt=<value> sets a scalar value, accessible to a script\n"
	   " under the name \"scriptopt\"\n\n");
    mpi_exit(err);
}

static void gretl_mpi_abort (char *line)
{
    fprintf(stderr, _("\ngretlcli-mpi: error executing script: halting\n"));
    fprintf(stderr, "> %s\n", line);
    mpi_exit(1);
}

static void noalloc (void)
{
    fputs(_("Out of memory!\n"), stderr);
    mpi_exit(1);
}

static int file_get_line (ExecState *s)
{
    char *line = s->line;
    int len = 0;

    memset(line, 0, MAXLINE);
    
    if (fgets(line, MAXLINE, fb) == NULL) {
	/* no more input from current source */
	gretl_exec_state_uncomment(s);
    } else {
	len = strlen(line);
    }

    if (*line == '\0') {
	strcpy(line, "quit");
    } else if (len == MAXLINE - 1 && line[len-1] != '\n') {
	return E_TOOLONG;
    } else {
	*linebak = '\0';
	strncat(linebak, line, MAXLINE-1);
	tailstrip(linebak);
    }

    if (gretl_echo_on() && s->cmd->ci == RUN && *line == '(') {
	printf("%s", line);
	*linebak = '\0';
    }

    return 0;
}

static void nls_init (void)
{
#ifdef ENABLE_NLS
# ifdef WIN32
    char LOCALEDIR[MAXLEN];

    build_path(LOCALEDIR, gretl_home(), "locale", NULL);
# endif /* WIN32 */

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE); 
    iso_gettext("@CLI_INIT");

    gretl_setenv("LC_NUMERIC", "");
    setlocale(LC_NUMERIC, "");
    reset_local_decpoint();
#endif /* ENABLE_NLS */
}

static int cli_clear_data (CMD *cmd, DATASET *dset, MODEL *model)
{
    gretlopt clearopt = 0;
    int err = 0;

    if (cmd->ci == CLEAR) {
	clearopt = cmd->opt;
    } else if (cmd->opt & OPT_P) {
	/* --preserve: clear dataset only */
	clearopt = OPT_D;
    } else if (csv_open_needs_matrix(cmd->opt)) {
	clearopt = OPT_D;
    }

    *datafile = '\0';

    if (dset->Z != NULL) {
	err = restore_full_sample(dset, NULL); 
	free_Z(dset); 
    }

    clear_datainfo(dset, CLEAR_FULL);
    data_status = 0;

    clear_model(model);

    if (clearopt & OPT_D) {
	libgretl_session_cleanup(SESSION_CLEAR_DATASET);
    } else {
	libgretl_session_cleanup(SESSION_CLEAR_ALL);
    }

    set_model_count(0);
    gretl_cmd_destroy_context(cmd);

    return err;
}

static int cli_get_input_line (ExecState *s)
{
    return file_get_line(s);
}

/* allow for continuation of lines */

static int maybe_get_input_line_continuation (char *line)
{
    char *test, tmp[MAXLINE];
    int contd, err = 0;

    if (!strncmp(line, "quit", 4)) {
	return 0;
    }

    contd = top_n_tail(line, MAXLINE, &err);

    while (contd && !err) {
	*tmp = '\0';
	test = fgets(tmp, MAXLINE, fb);
	if (test == NULL) {
	    break;
	}	
	if (*tmp != '\0') {
	    if (strlen(line) + strlen(tmp) > MAXLINE - 1) {
		err = E_TOOLONG;
		break;
	    } else {
		strcat(line, tmp);
		compress_spaces(line);
	    }
	}
	contd = top_n_tail(line, MAXLINE, &err);
    }

    return err;
}

static void maybe_print_intro (int id)
{
    if (id == 0) {
	printf("gretlcli-mpi %s\n", GRETL_VERSION);
	session_time(NULL);
    }
}

int main (int argc, char *argv[])
{
#ifdef WIN32
    char *callname = argv[0];
#endif
    char linecopy[MAXLINE];
    DATASET *dset = NULL;
    MODEL *model = NULL;
    ExecState state;
    char *line = NULL;
    char filearg[MAXLEN];
    char runfile[MAXLEN];
    double scriptval = NADBL;
    gretlopt progopt = 0;
    int use_dcmt = 1;
    CMD cmd;
    PRN *prn = NULL;
    PRN *cmdprn = NULL;
    int id, np;
    int err = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    MPI_Comm_size(MPI_COMM_WORLD, &np);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

#ifdef G_OS_WIN32
    win32_set_gretldir(callname);
#endif

    nls_init();

    dset = datainfo_new();
    if (dset == NULL) {
	noalloc();
    }

    if (argc < 2) {
	usage(1);
    } else {
	err = parse_options(&argc, &argv, &progopt, &scriptval, 
			    &use_dcmt, filearg);

	if (err) {
	    /* bad option, or missing filename */
	    usage(1);
	} else if (progopt & (OPT_HELP | OPT_VERSION)) {
	    /* we'll exit in these cases */
	    if (progopt & OPT_HELP) {
		usage(0);
	    } else {
		logo(0);
		mpi_exit(0);
	    }
	}

	strcpy(runfile, filearg);
	    
	if (progopt & OPT_ENGLISH) {
	    force_language(LANG_C);
	}
    }

    err = libgretl_mpi_init(id, np, use_dcmt);
    if (err) {
	fputs("Couldn't initialize the MPI sub-system\n", stderr);
	mpi_exit(1);
    }

    if (!(progopt & OPT_QUIET)) {
	maybe_print_intro(id);
    }

    prn = gretl_print_new(GRETL_PRINT_STDOUT, &err);
    if (err) {
	noalloc();
    }

    line = malloc(MAXLINE);
    if (line == NULL) {
	noalloc();
    }

#ifdef WIN32
    win32_cli_read_rc(callname);
#else
    cli_read_rc();
#endif /* WIN32 */

    /* allocate memory for model */
    model = allocate_working_model();
    if (model == NULL) {
	noalloc(); 
    }

    gretl_cmd_init(&cmd);
    gretl_exec_state_init(&state, 0, line, &cmd, model, prn);

    if (!na(scriptval)) {
	gretl_scalar_add("scriptopt", scriptval);
    }

    runit = 0;
    sprintf(line, "run %s\n", runfile);
    state.flags |= INIT_EXEC;
    err = cli_exec_line(&state, id, dset, progopt, cmdprn);
    state.flags ^= INIT_EXEC;
    if (err && fb == NULL) {
	mpi_exit(1);
    }

    *linecopy = '\0';

    /* enter main command loop */

    while (cmd.ci != QUIT && fb != NULL) {
	if (err) {
	    gretl_mpi_abort(linecopy);
	}

	if (gretl_execute_loop()) { 
	    err = gretl_loop_exec(&state, dset);
	    if (err) {
		break;
	    }
	} else {
	    err = cli_get_input_line(&state);
	    if (err) {
		errmsg(err, prn);
		break;
	    }
	}

	if (!state.in_comment) {
	    if (cmd.context == FOREIGN || gretl_compiling_python(line)) {
		tailstrip(line);
	    } else {
		err = maybe_get_input_line_continuation(line); 
		if (err) {
		    errmsg(err, prn);
		    break;
		}
	    }
	} 

	strcpy(linecopy, line);
	tailstrip(linecopy);
	err = cli_exec_line(&state, id, dset, progopt, cmdprn);
    }

    /* finished main command loop */

    if (!err) {
	err = gretl_if_state_check(0);
	if (err) {
	    errmsg(err, prn);
	}
    }

    if (err) {
	mpi_exit(err);
    }

    /* leak check -- explicitly free all memory allocated */

    destroy_working_model(model);
    destroy_dataset(dset);

    if (fb != stdin && fb != NULL) {
	fclose(fb);
    }

    free(line);

    gretl_print_destroy(prn);
    gretl_cmd_free(&cmd);
    libgretl_cleanup();

    MPI_Finalize();

    return 0;
}

static void printline (const char *s)
{
    if (gretl_compiling_loop()) {
	printf("> %s\n", s);
    } else {
	printf("%s\n", s);
    }
}

static void cli_exec_callback (ExecState *s, void *ptr,
			       GretlObjType type)
{
    int ci = s->cmd->ci;

    if (ci == MODELTAB || ci == GRAPHPG) {
	pprintf(s->prn, _("%s: command not available\n"), s->cmd->word);
    } else if (ci == OPEN) {
	char *fname = (char *) ptr;

	if (fname != NULL && *fname != '\0') {
	    strncpy(datafile, fname, MAXLEN - 1);
	}
	data_status = 1;
    }

    /* otherwise, no-op */
}

static int cli_renumber_series (const char *s, DATASET *dset, 
				PRN *prn)
{
    int err, fixmax = highest_numbered_var_in_saved_object(dset);

    err = renumber_series_with_checks(s, fixmax, dset, prn);
    if (err) {
	errmsg(err, prn);
    }

    return err;
}

static int cli_try_http (const char *s, char *fname, int *http)
{
    int err = 0;

    /* skip past command word */
    s += strcspn(s, " ");
    s += strspn(s, " ");

    if (strncmp(s, "http://", 7) == 0) {
#ifdef USE_CURL
	err = retrieve_public_file(s, fname);
	if (!err) {
	    *http = 1;
	}
#else
	gretl_errmsg_set(_("Internet access not supported"));
	err = E_DATA;
#endif
    }

    return err;
}

static int cli_open_append (CMD *cmd, const char *line, 
			    DATASET *dset, MODEL *model,
			    PRN *prn)
{
    gretlopt opt = cmd->opt;
    PRN *vprn = prn;
    char newfile[MAXLEN] = {0};
    int http = 0, dbdata = 0;
    int ftype;
    int err = 0;

    err = cli_try_http(line, newfile, &http);
    if (err) {
	errmsg(err, prn);
	return err;
    }

    if (!http && !(opt & OPT_O)) {
	/* not using http or ODBC */
	err = getopenfile(line, newfile, (opt & OPT_W)?
			  OPT_W : OPT_NONE);
	if (err) {
	    errmsg(err, prn);
	    return err;
	}
    }
    
    if (opt & OPT_W) {
	ftype = GRETL_NATIVE_DB_WWW;
    } else if (opt & OPT_O) {
	ftype = GRETL_ODBC;
    } else {
	ftype = detect_filetype(newfile, OPT_P);
    }

    dbdata = (ftype == GRETL_NATIVE_DB || ftype == GRETL_NATIVE_DB_WWW ||
	      ftype == GRETL_RATS_DB || ftype == GRETL_PCGIVE_DB ||
	      ftype == GRETL_ODBC);

    if (!dbdata && cmd->ci != APPEND) {
	cli_clear_data(cmd, dset, model);
    } 

    if (opt & OPT_Q) {
	/* --quiet, but in case we hit any problems below... */
	vprn = gretl_print_new(GRETL_PRINT_BUFFER, NULL);
    }

    if (ftype == GRETL_XML_DATA || ftype == GRETL_BINARY_DATA) {
	err = gretl_read_gdt(newfile, dset, opt, vprn);
    } else if (ftype == GRETL_CSV) {
	err = import_csv(newfile, dset, opt, vprn);
    } else if (SPREADSHEET_IMPORT(ftype)) {
	err = import_spreadsheet(newfile, ftype, cmd->list, cmd->parm2,
				 dset, opt, vprn);
    } else if (OTHER_IMPORT(ftype)) {
	err = import_other(newfile, ftype, dset, opt, vprn);
    } else if (ftype == GRETL_ODBC) {
	err = set_odbc_dsn(line, vprn);
    } else if (dbdata) {
	err = set_db_name(newfile, ftype, vprn);
    } else {
	err = gretl_get_data(newfile, dset, opt, vprn);
    }

    if (vprn != prn) {
	if (err) {
	    /* The user asked for --quiet operation, but something
	       went wrong so let's print any info we got on
	       vprn.
	    */
	    const char *buf = gretl_print_get_buffer(vprn);

	    if (buf != NULL && *buf != '\0') {
		pputs(prn, buf);
	    }
	} else {
	    /* print minimal success message */
	    pprintf(prn, _("Read datafile %s\n"), newfile);
	}
	gretl_print_destroy(vprn);
    }

    if (err) {
	errmsg(err, prn);
	return err;
    }

    if (!dbdata && !http && cmd->ci != APPEND) {
	strncpy(datafile, newfile, MAXLEN - 1);
    }

    data_status = 1;

    if (dset->v > 0 && !dbdata && !(opt & OPT_Q)) {
	varlist(dset, prn);
    }

    if (http) {
	remove(newfile);
    }

    return err;
}

static void maybe_save_session_output (const char *cmdfile)
{
    char outfile[FILENAME_MAX];

    printf(_("type a filename to store output (enter to quit): "));

    *outfile = '\0';

    if (fgets(outfile, sizeof outfile, stdin) != NULL) {
	top_n_tail(outfile, 0, NULL);
    }

    if (*outfile != '\0' && *outfile != '\n' && *outfile != '\r' 
	&& strcmp(outfile, "q")) {
	const char *udir = gretl_workdir();
	char *syscmd;

	printf(_("writing session output to %s%s\n"), udir, outfile);
#ifdef WIN32
	syscmd = gretl_strdup_printf("\"%sgretlcli\" -b \"%s\" > \"%s%s\"", 
				     gretl_home(), cmdfile, udir, outfile);
	system(syscmd);
#else
	syscmd = gretl_strdup_printf("gretlcli -b \"%s\" > \"%s%s\"", 
				     cmdfile, udir, outfile);
	gretl_spawn(syscmd);
#endif
	printf("%s\n", syscmd);
	free(syscmd);
    }
}

#define ENDRUN (NC + 1)

/* cli_exec_line: this is called to execute both interactive and
   script commands.  Note that most commands get passed on to the
   libgretl function gretl_cmd_exec(), but some commands that require
   special action are dealt with here.

   see also gui_exec_line() in gui2/library.c
*/

static int cli_exec_line (ExecState *s, int id, DATASET *dset, 
			  gretlopt progopt, PRN *cmdprn)
{
    char *line = s->line;
    CMD *cmd = s->cmd;
    PRN *prn = s->prn;
    MODEL *model = s->model;
    char runfile[MAXLEN];
    int err = 0;

    if (gretl_compiling_function()) {
	err = gretl_function_append_line(line);
	if (err) {
	    errmsg(err, prn);
	}
	return err;
    }

    if (string_is_blank(line)) {
	return 0;
    }

    if (!s->in_comment && !cmd->context && !gretl_if_state_false()) {
	/* catch requests relating to saved objects, which are not
	   really "commands" as such */
	int action = cli_saved_object_action(line, dset, prn);

	if (action == OBJ_ACTION_INVALID) {
	    return 1; /* action was faulty */
	} else if (action != OBJ_ACTION_NONE) {
	    return 0; /* action was OK (and handled), or ignored */
	}
    }

    /* tell libgretl that we're in batch mode */
    gretl_set_batch_mode(1);

    if (gretl_compiling_loop()) {
	/* if we're stacking commands for a loop, parse "lightly" */
	err = get_command_index(line, cmd);
    } else {
	err = parse_command_line(line, cmd, dset, NULL);
    }

    if (err) {
	int catch = 0;

	gretl_exec_state_uncomment(s);
	if (err != E_ALLOC && (cmd->flags & CMD_CATCH)) {
	    set_gretl_errno(err);
	    catch = 1;
	}
        errmsg(err, prn);
	return (catch)? 0 : err;
    }

    gretl_exec_state_transcribe_flags(s, cmd);

    /* echo comments from input */
    if (runit < 2 && cmd->ci == CMD_COMMENT && gretl_echo_on()) {
	printline(linebak);
    }

    if (cmd->ci < 0) {
	/* nothing there, comment, or masked by "if" */
	return 0;
    }

    if (s->sys != NULL && cmd->ci != END && cmd->ci != EQUATION &&
	cmd->ci != SYSTEM) {
	printf(_("Command '%s' ignored; not valid within equation system\n"), 
	       line);
	equation_system_destroy(s->sys);
	s->sys = NULL;
	return 1;
    }

    if (cmd->ci == LOOP || gretl_compiling_loop()) {  
	/* accumulating loop commands */
	if (gretl_echo_on()) {
	    /* straight visual echo */
	    echo_command(cmd, dset, line, prn);
	}
	err = gretl_loop_append_line(s, dset);
	if (err) {
	    errmsg(err, prn);
	}
	return err;
    }

    if (gretl_echo_on() && !(s->flags & INIT_EXEC)) {
	/* visual feedback, not recording */
	if (cmd->ci == FUNC && runit > 1) {
	    ; /* don't echo */
	} else {
	    echo_command(cmd, dset, line, prn);
	}
    }

    check_for_loop_only_options(cmd->ci, cmd->opt, prn);

    gretl_exec_state_set_callback(s, cli_exec_callback, OPT_NONE);

    switch (cmd->ci) {

    case DELEET:
	if (cmd->opt & OPT_D) {
	    /* delete from database */
	    err = db_delete_series_by_name(cmd->param, prn);
	} else if (cmd->opt & OPT_T) {
	    /* delete all by type (not series) */
	    err = gretl_cmd_exec(s, dset);
	} else if (*cmd->param != '\0') {
	    /* got a named non-series variable */
	    err = gretl_delete_var_by_name(cmd->param, prn);
	} else if (*cmd->parm2 != '\0') {
	    /* "extra" means we got "list <listname> delete" */
	    if (get_list_by_name(cmd->parm2)) {
		err = user_var_delete_by_name(cmd->parm2, prn);
	    } else {
		err = E_UNKVAR;
	    }
	} else {
	    /* list of series? */
	    int nv = 0;

	    err = dataset_drop_listed_variables(cmd->list, dset, 
						&nv, prn);
	}
	if (err) {
	    errmsg(err, prn);
	} 
	break;

    case HELP:
	cli_help(cmd->param, cmd->opt, prn);
	break;

    case OPEN:
    case APPEND:
	err = cli_open_append(cmd, line, dset, model, prn);
	break;

    case NULLDATA:
	if (cmd->order < 1) {
	    err = 1;
	    pputs(prn, _("Data series length count missing or invalid\n"));
	} else {
	    cli_clear_data(cmd, dset, model);
	    err = open_nulldata(dset, data_status, cmd->order, 
				cmd->opt, prn);
	    if (err) { 
		errmsg(err, prn);
	    } else {
		data_status = 1;
	    }
	}
	break;

    case QUIT:
	if (runit) {
	    *s->runfile = '\0';
	    runit--;
	    fclose(fb);
	    fb = pop_input_file();
	    if (fb == NULL) {
		if (id == 0 && !(progopt & OPT_QUIET)) {
		    pputs(prn, _("Done\n"));
		}
	    } else {
		cmd->ci = ENDRUN;
	    }
	} else {
	    printf(_("commands saved as %s\n"), cmdfile);
	    gretl_print_destroy(cmdprn);
	    if (!(cmd->opt & OPT_X)) {
		maybe_save_session_output(cmdfile);
	    }
	}
	break;

    case RUN:
    case INCLUDE:
	if (cmd->ci == INCLUDE) {
	    err = getopenfile(line, runfile, OPT_I);
	} else {
	    err = getopenfile(line, runfile, OPT_S);
	}
	if (err) { 
	    errmsg(err, prn);
	    break;
	}
	if (gretl_messages_on() && !(s->flags & INIT_EXEC)) {
	    pprintf(prn, " %s\n", runfile);
	}
	if (cmd->ci == INCLUDE && gretl_is_xml_file(runfile)) {
	    err = load_user_XML_file(runfile, prn);
	    if (err) {
		pprintf(prn, _("Error reading %s\n"), runfile);
	    } else {
		pprintf(cmdprn, "include \"%s\"\n", runfile);
	    }
	    break;
	}
	if (!strcmp(runfile, s->runfile)) { 
	    pprintf(prn, _("Infinite loop detected in script\n"));
	    err = 1;
	    break;
	}
	if (fb != NULL) {
	    push_input_file(fb);
	}
	if ((fb = fopen(runfile, "r")) == NULL) {
	    pprintf(prn, _("Error reading %s\n"), runfile);
	    err = 1;
	    fb = pop_input_file();
	} else {
	    gretl_set_current_dir(runfile);
	    strcpy(s->runfile, runfile);
	    if (cmd->ci == INCLUDE) {
		pprintf(cmdprn, "include \"%s\"\n", runfile);
	    } else {
		pprintf(cmdprn, "run \"%s\"\n", runfile);
	    }
	    runit++;
	}
	break;

    case CLEAR:
	err = cli_clear_data(cmd, dset, model);
	break;

    case DATAMOD:
	if (cmd->aux == DS_CLEAR) {
	    err = cli_clear_data(cmd, dset, model);
	    pputs(prn, _("Dataset cleared\n"));
	    break;
	} else if (cmd->aux == DS_RENUMBER) {
	    err = cli_renumber_series(cmd->param, dset, prn);
	    break;
	}
	/* else fall-through intended */

    default:
	err = gretl_cmd_exec(s, dset);
	break;
    }

    if (err) {
	gretl_exec_state_uncomment(s);
    }

    return err;
}

/* apparatus for keeping track of input stream */

#define N_STACKED_FILES 8

static int nfiles;
static FILE *fstack[N_STACKED_FILES];

static int push_input_file (FILE *fp)
{
    int err = 0;

    if (nfiles >= N_STACKED_FILES) {
	err = 1;
    } else {
	fstack[nfiles++] = fp;
    }

    return err;
}

static FILE *pop_input_file (void)
{
    FILE *ret = NULL;

    if (nfiles > 0) {
	ret = fstack[--nfiles];
    }

    return ret;
}

#include "cli_object.c"
