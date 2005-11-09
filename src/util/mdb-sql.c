/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>

#include <stdio.h>

#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)
#    include <readline.h>
#  else
/* no readline.h */
extern char *readline ();
#  endif
char *cmdline = NULL;
#endif /* HAVE_LIBREADLINE */

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  else
/* no history.h */
extern void add_history ();
extern int write_history ();
extern int read_history ();
extern void clear_history ();
#  endif
#endif /* HAVE_READLINE_HISTORY */

#include <string.h>
#include "mdbsql.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void dump_results(FILE *out, MdbSQL *sql, char *delimiter);
void dump_results_pp(FILE *out, MdbSQL *sql);
int yyparse(void);

#if SQL

int headers = 1;
int footers = 1;
int pretty_print = 1;
int showplan = 0;
int noexec = 0;

#ifdef HAVE_READLINE_HISTORY
#define HISTFILE ".mdbhistory"
#endif

#ifndef HAVE_LIBREADLINE
char *readline(char *prompt)
{
char *buf, line[1000];
int i = 0;

	printf("%s",prompt);
	if (! fgets(line,1000,stdin)) {
		return NULL;
	}
	for (i=strlen(line);i>0;i--) {
		if (line[i]=='\n') {
			line[i]='\0';
			break;
		}
	}
	buf = (char *) malloc(strlen(line)+1);
	strcpy(buf,line);

	return buf;
}
#endif

int parse(MdbSQL *sql, char *buf)
{
	g_input_ptr = buf;
	/* begin unsafe */
	_mdb_sql(sql);
	if (yyparse()) {
		/* end unsafe */
		fprintf(stderr, "Couldn't parse SQL\n");
		mdb_sql_reset(sql);
		return 1;
	} else {
		return 0;
	}
}

void
do_set_cmd(MdbSQL *sql, char *s)
{
	char *level1, *level2;
	level1 = strtok(s, " \t\n");
	if (!level1) {
		printf("Usage: set [stats|showplan|noexec] [on|off]\n");
		return;
	}
	if (!strcmp(level1,"stats")) {
		level2 = strtok(NULL, " \t");
		if (!level2) {
			printf("Usage: set stats [on|off]\n");
			return;
		}
		if (!strcmp(level2,"on")) {
			mdb_stats_on(sql->mdb);
		} else if (!strcmp(level2,"off")) {
			mdb_stats_off(sql->mdb);
			mdb_dump_stats(sql->mdb);
		} else {
			printf("Unknown stats option %s\n", level2);
			printf("Usage: set stats [on|off]\n");
		}
	} else if (!strcmp(level1,"showplan")) {
		level2 = strtok(NULL, " \t");
		if (!level2) {
			printf("Usage: set showplan [on|off]\n");
			return;
		}
		if (!strcmp(level2,"on")) {
			showplan=1;
		} else if (!strcmp(level2,"off")) {
			showplan=0;
		} else {
			printf("Unknown showplan option %s\n", level2);
			printf("Usage: set showplan [on|off]\n");
		}
	} else if (!strcmp(level1,"noexec")) {
		level2 = strtok(NULL, " \t");
		if (!level2) {
			printf("Usage: set noexec [on|off]\n");
			return;
		}
		if (!strcmp(level2,"on")) {
			noexec=1;
		} else if (!strcmp(level2,"off")) {
			noexec=0;
		} else {
			printf("Unknown noexec option %s\n", level2);
			printf("Usage: set noexec [on|off]\n");
		}
	} else {
		printf("Unknown set command %s\n", level1);
		printf("Usage: set [stats|showplan|noexec] [on|off]\n");
	}
}

int
read_file(char *s, int line, unsigned int *bufsz, char *mybuf)
{
	char *fname;
	FILE *in;
	char buf[256];
	unsigned int cursz = 0;	
	int lines = 0;	

	fname = s;
	while (*fname && *fname==' ') fname++;

	if (! (in = fopen(fname, "r"))) {
		fprintf(stderr,"Unable to open file %s\n", fname);
		mybuf[0]=0;
		return 0;
	}
	while (fgets(buf, 255, in)) {
		cursz += strlen(buf) + 1;
		if (cursz > (*bufsz)) {
			(*bufsz) *= 2;
			mybuf = (char *) realloc(mybuf, *bufsz);
		}	
		strcat(mybuf, buf);
#ifdef HAVE_READLINE_HISTORY
		/* don't record blank lines */
		if (strlen(buf)) add_history(buf);
#endif
		strcat(mybuf, "\n");
		lines++;
		printf("%d => %s",line+lines, buf);
	}
	return lines;
}
void 
run_query(FILE *out, MdbSQL *sql, char *mybuf, char *delimiter)
{
	MdbTableDef *table;

	if (!parse(sql, mybuf) && sql->cur_table) {
		if (showplan) {
			table = sql->cur_table;
			if (table->sarg_tree) mdb_sql_dump_node(table->sarg_tree, 0);
			if (sql->cur_table->strategy == MDB_TABLE_SCAN)
				printf("Table scanning %s\n", table->name);
			else 
				printf("Index scanning %s using %s\n", table->name, table->scan_idx->name);
		}
		if (noexec) {
			mdb_sql_reset(sql);
			return;
		}
		mdb_sql_bind_all(sql);
		if (pretty_print)
			dump_results_pp(out, sql);
		else
			dump_results(out, sql, delimiter);
	}
}

void print_value(FILE *out, char *v, int sz, int first)
{
int i;
int vlen;

	if (first) {
		fprintf(out,"|");
	}
	vlen = strlen(v);
	for (i=0;i<sz;i++) {
		fprintf(out,"%c",i >= vlen ? ' ' : v[i]);
	}
	fprintf(out,"|");
}
static void print_break(FILE *out, int sz, int first)
{
int i;
	if (first) {
		fprintf(out,"+");
	}
	for (i=0;i<sz;i++) {
		fprintf(out,"-");
	}
	fprintf(out,"+");
}
void print_rows_retrieved(FILE *out, unsigned long row_count)
{
	if (!row_count) 
		fprintf(out, "No Rows retrieved\n");
	else if (row_count==1)
		fprintf(out, "1 Row retrieved\n");
	else 
		fprintf(out, "%lu Rows retrieved\n", row_count);
}
void
dump_results(FILE *out, MdbSQL *sql, char *delimiter)
{
	unsigned int j;
	MdbSQLColumn *sqlcol;
	unsigned long row_count = 0;

	if (headers) {
		for (j=0;j<sql->num_columns-1;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			fprintf(out, "%s%s", sqlcol->name,
				delimiter ? delimiter : "\t");
		}
		sqlcol = g_ptr_array_index(sql->columns,sql->num_columns-1);
		fprintf(out, "%s", sqlcol->name);
		fprintf(out,"\n");
	}
	while(mdb_fetch_row(sql->cur_table)) {
		row_count++;
  		for (j=0;j<sql->num_columns-1;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			fprintf(out, "%s%s", sql->bound_values[j],
				delimiter ? delimiter : "\t");
		}
		sqlcol = g_ptr_array_index(sql->columns,sql->num_columns-1);
		fprintf(out, "%s", sql->bound_values[sql->num_columns-1]);
		fprintf(out,"\n");
	}
	if (footers) {
		print_rows_retrieved(out, row_count);
	}

	mdb_sql_reset(sql);
}

void 
dump_results_pp(FILE *out, MdbSQL *sql)
{
	unsigned int j;
	MdbSQLColumn *sqlcol;
	unsigned long row_count = 0;

	/* print header */
	if (headers) {
		for (j=0;j<sql->num_columns;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			if (strlen(sqlcol->name)>sqlcol->disp_size)
				sqlcol->disp_size = strlen(sqlcol->name);
			print_break(out, sqlcol->disp_size, !j);
		}
		fprintf(out,"\n");
		for (j=0;j<sql->num_columns;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			print_value(out, sqlcol->name,sqlcol->disp_size,!j);
		}
		fprintf(out,"\n");
	}

	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(out, sqlcol->disp_size, !j);
	}
	fprintf(out,"\n");

	/* print each row */
	while(mdb_fetch_row(sql->cur_table)) {
		row_count++;
  		for (j=0;j<sql->num_columns;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			print_value(out, sql->bound_values[j],sqlcol->disp_size,!j);
		}
		fprintf(out,"\n");
	}

	/* footer */
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(out, sqlcol->disp_size, !j);
	}
	fprintf(out,"\n");
	if (footers) {
		print_rows_retrieved(out, row_count);
	}

	/* clean up */
	for (j=0;j<sql->num_columns;j++) {
		g_free(sql->bound_values[j]);
	}

	mdb_sql_reset(sql);
}

int
main(int argc, char **argv)
{
char *s = NULL;
char prompt[20];
int line = 0;
char *mybuf;
unsigned int bufsz;
MdbSQL *sql;
int opt;
FILE *in = NULL, *out = NULL;
char *home = getenv("HOME");
char *histpath;
char *delimiter = NULL;


#ifdef HAVE_READLINE_HISTORY
	if (home) {
		histpath = (char *) g_strconcat(home, "/", HISTFILE, NULL);
		read_history(histpath);
		g_free(histpath);
	}
#endif
	if (!isatty(fileno(stdin))) {
		in = stdin;
	}
	while ((opt=getopt(argc, argv, "HFpd:i:o:"))!=-1) {
		switch (opt) {
		        case 'd':
				delimiter = (char *) g_strdup(optarg);
				break;
		        case 'p':
				pretty_print=0;
				break;
		        case 'H':
				headers=0;
				break;
		        case 'F':
				footers=0;
				break;
		        case 'i':
				if (!strcmp(optarg, "stdin"))
					in = stdin;
				else if (!(in = fopen(optarg, "r"))) {
					fprintf(stderr,"Unable to open file %s\n", optarg);
					exit(1);
				}
				break;
		        case 'o':
				if (!(out = fopen(optarg, "w"))) {
					fprintf(stderr,"Unable to open file %s\n", optarg);
					exit(1);
				}
				break;
			default:
				fprintf(stdout,"Unknown option.\nUsage: %s [-HFp] [-d <delimiter>] [-i <file>] [-o <file>] [<database>]\n", argv[0]);
				exit(1);
		}
	}
	/* initialize the SQL engine */
	sql = mdb_sql_init();
	if (argc>optind) {
		mdb_sql_open(sql, argv[optind]);
	}

	/* give the buffer an initial size */
	bufsz = 4096;
	mybuf = (char *) g_malloc(bufsz);
	mybuf[0]='\0';

	while (1) {
		line ++;
		if (s) free(s);

		if (in) {
			s=malloc(256);
			if ((!s) || (!fgets(s, 256, in))) {
				/* if we have something in the buffer, run it */
				if (strlen(mybuf))
					run_query((out) ? out : stdout,
					          sql, mybuf, delimiter);
				break;
			}
			if (s[strlen(s)-1]=='\n')
				s[strlen(s)-1]=0;
		} else {
			sprintf(prompt, "%d => ", line);
			s=readline(prompt);
			if (!s)
				break;
		}

		if (!strcmp(s,"exit") || !strcmp(s,"quit") || !strcmp(s,"bye"))
			break;

		if (line==1 && (!strncmp(s,"set ",4) || !strcmp(s,"set"))) {
			do_set_cmd(sql, &s[3]);
			line = 0;
		} else if (!strcmp(s,"go")) {
			line = 0;
			run_query((out) ? out : stdout, sql, mybuf, delimiter);
			mybuf[0]='\0';
		} else if (!strcmp(s,"reset")) {
			line = 0;
			mybuf[0]='\0';
		} else if (!strncmp(s,":r",2)) {
			line += read_file(&s[2], line, &bufsz, mybuf);
		} else {
			while (strlen(mybuf) + strlen(s) > bufsz) {
				bufsz *= 2;
				mybuf = (char *) g_realloc(mybuf, bufsz);
			}
#ifdef HAVE_READLINE_HISTORY
			/* don't record blank lines */
			if (strlen(s)) add_history(s);
#endif
			strcat(mybuf,s);
			/* preserve line numbering for the parser */
			strcat(mybuf,"\n");
		}
	}
	mdb_sql_exit(sql);	

	g_free(mybuf);
	g_free(delimiter);
	if (s) free(s);
	if (out) fclose(out);
	if ((in) && (in != stdin)) fclose(in);

#ifdef HAVE_READLINE_HISTORY
	if (home) {
		histpath = (char *) g_strconcat(home, "/", HISTFILE, NULL);
		write_history(histpath);
		g_free(histpath);
		clear_history();
	}
#endif

	exit(0);
}
#else
int main(int argc, char **argv)
{
	fprintf(stderr,"You must configure using --enable-sql to get SQL support\n");

	exit(-1);
}
#endif
