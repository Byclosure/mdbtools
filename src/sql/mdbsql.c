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

#include "mdbsql.h"
#include <stdarg.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void mdb_dump_results(MdbSQL *sql);

#ifdef HAVE_WORDEXP_H
#define HAVE_WORDEXP
#include <wordexp.h>
#endif

void
mdb_sql_error(char *fmt, ...)
{
va_list ap;

	va_start(ap, fmt);
	vfprintf (stderr,fmt, ap);
	va_end(ap);
	fprintf(stderr,"\n");
}
int mdb_sql_yyinput(char *buf, int need)
{
int cplen, have;

	have = strlen(g_input_ptr);
	cplen = need > have ? have : need;
	
	if (cplen>0) {
		memcpy(buf, g_input_ptr, cplen);
		g_input_ptr += cplen;
	} 
	return cplen;
}
MdbSQL *mdb_sql_init()
{
MdbSQL *sql;

	mdb_init();
	sql = (MdbSQL *) g_malloc0(sizeof(MdbSQL));
	sql->columns = g_ptr_array_new();
	sql->tables = g_ptr_array_new();
	sql->sargs = g_ptr_array_new();
	sql->sarg_tree = NULL;
	sql->sarg_stack = NULL;

	return sql;
}

MdbSQLSarg *mdb_sql_alloc_sarg()
{
MdbSQLSarg *sql_sarg;
	sql_sarg = (MdbSQLSarg *) malloc(sizeof(MdbSQLSarg));
	memset(sql_sarg,0,sizeof(MdbSQLSarg));
	sql_sarg->sarg = (MdbSarg *) malloc(sizeof(MdbSarg));
	memset(sql_sarg->sarg,0,sizeof(MdbSarg));
	return sql_sarg;
}
void mdb_sql_free_column(MdbSQLColumn *c)
{
	if (c->name) g_free(c->name);
	g_free(c);
}
MdbSQLColumn *mdb_sql_alloc_column()
{
MdbSQLColumn *c;

	c = (MdbSQLColumn *) g_malloc(sizeof(MdbSQLColumn));
	memset(c,0,sizeof(MdbSQLColumn));
	return c;
}
void mdb_sql_free_table(MdbSQLTable *t)
{
	if (t->name) g_free(t->name);
	g_free(t);
}
MdbSQLTable *mdb_sql_alloc_table()
{
MdbSQLTable *t;

	t = (MdbSQLTable *) g_malloc(sizeof(MdbSQLTable));
	memset(t,0,sizeof(MdbSQLTable));
	return t;
}

void
mdb_sql_close(MdbSQL *sql)
{
	if (sql->mdb) {
		mdb_close(sql->mdb);
		mdb_free_handle(sql->mdb);
		sql->mdb = NULL;
	} else {
		mdb_sql_error("Not connected.");
	}
}

MdbHandle *mdb_sql_open(MdbSQL *sql, char *db_name)
{
int fail = 0;
char *db_namep = db_name;

#ifdef HAVE_WORDEXP
wordexp_t words;

	if (wordexp(db_name, &words, 0)==0) {
		if (words.we_wordc>0) 
			db_namep = words.we_wordv[0];
	}
	
#endif

	if (!(sql->mdb = mdb_open(db_namep))) {
		if (!strstr(db_namep, ".mdb")) {
			char *tmpstr = (char *) malloc(strlen(db_namep)+5);
			strcpy(tmpstr,db_namep);
			strcat(tmpstr,".mdb");
			if (!(sql->mdb = mdb_open(tmpstr))) {
				fail++;
			}
			free(tmpstr);
		} else {
			fail++;
		}
	}
	if (fail) {
		mdb_sql_error("Unable to locate database %s", db_name);
	}

#ifdef HAVE_WORDEXP
	wordfree(&words);
#endif	

	return sql->mdb;
}
MdbSargNode *
mdb_sql_alloc_node()
{
	MdbSargNode *node;

	node = g_malloc0(sizeof(MdbSargNode));

	return node;
}
void
mdb_sql_free_tree(MdbSargNode *tree)
{

	if (tree->left) mdb_sql_free_tree(tree->left);
	if (tree->right) mdb_sql_free_tree(tree->right);
	g_free(tree);
}
void
mdb_sql_push_node(MdbSQL *sql, MdbSargNode *node)
{
	sql->sarg_stack = g_list_append(sql->sarg_stack, node);
	/*
	 * Tree builds from bottom to top, so we should be left with
	 * the correct tree root when done
	 */
	sql->sarg_tree = node;
}
MdbSargNode *
mdb_sql_pop_node(MdbSQL *sql)
{
	GList *glist;
	MdbSargNode *node;

	glist = g_list_last(sql->sarg_stack);
	if (!glist) return NULL;
	
	node = glist->data;
#if 0
	if (node->op==MDB_EQUAL) 
		printf("popping %d\n", node->value.i);
	else
		printf("popping %s\n", node->op == MDB_OR ? "OR" : "AND");
#endif
	sql->sarg_stack = g_list_remove(sql->sarg_stack, node);
	return node;
}

void 
mdb_sql_add_not(MdbSQL *sql)
{
	MdbSargNode *node, *left;

	left = mdb_sql_pop_node(sql);
	if (!left) {
		mdb_sql_error("parse error near 'NOT'");
		mdb_sql_reset(sql);
		return;
	}
	node = mdb_sql_alloc_node();
	node->op = MDB_NOT;
	node->left = left;
	mdb_sql_push_node(sql, node);
}
void 
mdb_sql_add_or(MdbSQL *sql)
{
	MdbSargNode *node, *left, *right;

	left = mdb_sql_pop_node(sql);
	right = mdb_sql_pop_node(sql);
	if (!left || !right) {
		mdb_sql_error("parse error near 'OR'");
		mdb_sql_reset(sql);
		return;
	}
	node = mdb_sql_alloc_node();
	node->op = MDB_OR;
	node->left = left;
	node->right = right;
	mdb_sql_push_node(sql, node);
}
void 
mdb_sql_add_and(MdbSQL *sql)
{
	MdbSargNode *node, *left, *right;

	left = mdb_sql_pop_node(sql);
	right = mdb_sql_pop_node(sql);
	if (!left || !right) {
		mdb_sql_error("parse error near 'AND'");
		mdb_sql_reset(sql);
		return;
	}
	node = mdb_sql_alloc_node();
	node->op = MDB_AND;
	node->left = left;
	node->right = right;
	mdb_sql_push_node(sql, node);
}
void 
mdb_sql_dump_node(MdbSargNode *node, int level)
{
	int i;
	int mylevel = level+1;

	if (!level) 
		printf("root  ");
	for (i=0;i<mylevel;i++) printf("--->");
	switch (node->op) {
		case MDB_OR: 
			printf(" or\n"); 
			break;
		case MDB_AND: 
			printf(" and\n"); 
			break;
		case MDB_NOT: 
			printf(" not\n"); 
			break;
		case MDB_LT: 
			printf(" < %d\n", node->value.i); 
			break;
		case MDB_GT: 
			printf(" < %d\n", node->value.i); 
			break;
		case MDB_LIKE: 
			printf(" like %s\n", node->value.s); 
			break;
		case MDB_EQUAL: 
			printf(" = %d\n", node->value.i); 
			break;
	}
	if (node->left) {
		printf("left  ");
		mdb_sql_dump_node(node->left, mylevel);
	}
	if (node->right) {
		printf("right ");
		mdb_sql_dump_node(node->right, mylevel);
	}
}
int 
mdb_sql_add_sarg(MdbSQL *sql, char *col_name, int op, char *constant)
{
	MdbSQLSarg *sql_sarg;
	int lastchar;
	MdbSargNode *node;

	node = mdb_sql_alloc_node();
	node->op = op;
	/* stash the column name until we finish with the grammar */
	node->parent = (void *) g_strdup(col_name);

	sql_sarg = mdb_sql_alloc_sarg();
	sql_sarg->col_name = g_strdup(col_name);
	sql_sarg->sarg->op = op;

	/* FIX ME -- we should probably just be storing the ascii value until the 
	** column definition can be checked for validity
	*/
	if (constant[0]=='\'') {
		lastchar = strlen(constant) > 256 ? 256 : strlen(constant);
		strncpy(sql_sarg->sarg->value.s, &constant[1], lastchar - 2);
		sql_sarg->sarg->value.s[lastchar - 1]='\0';
		strncpy(node->value.s, &constant[1], lastchar - 2);;
		node->value.s[lastchar - 1]='\0';
	} else {
		sql_sarg->sarg->value.i = atoi(constant);
		node->value.i = atoi(constant);
	}
	g_ptr_array_add(sql->sargs, sql_sarg);
	sql->num_sargs++;

	mdb_sql_push_node(sql, node);

	return 0;
}
void
mdb_sql_all_columns(MdbSQL *sql)
{
	sql->all_columns=1;	
}
int mdb_sql_add_column(MdbSQL *sql, char *column_name)
{
MdbSQLColumn *c;

	c = mdb_sql_alloc_column();
	c->name = g_strdup(column_name);
	g_ptr_array_add(sql->columns, c);
	sql->num_columns++;
	return 0;
}
int mdb_sql_add_table(MdbSQL *sql, char *table_name)
{
MdbSQLTable *t;

	t = mdb_sql_alloc_table();
	t->name = g_strdup(table_name);
	t->alias = NULL;
	g_ptr_array_add(sql->tables, t);
	sql->num_tables++;
	return 0;
}
void mdb_sql_dump(MdbSQL *sql)
{
int i;
MdbSQLColumn *c;
MdbSQLTable *t;

	for (i=0;i<sql->num_columns;i++) {
		c = g_ptr_array_index(sql->columns,i);
		printf("column = %s\n",c->name);
	}
	for (i=0;i<sql->num_tables;i++) {
		t = g_ptr_array_index(sql->tables,i);
		printf("table = %s\n",t->name);
	}
}
void mdb_sql_exit(MdbSQL *sql)
{
int i;
MdbSQLColumn *c;
MdbSQLTable *t;
MdbSQLSarg *sql_sarg;

	for (i=0;i<sql->num_columns;i++) {
		c = g_ptr_array_index(sql->columns,i);
		if (c->name) g_free(c->name);
	}
	for (i=0;i<sql->num_tables;i++) {
		t = g_ptr_array_index(sql->tables,i);
		if (t->name) g_free(t->name);
	}
	for (i=0;i<sql->num_sargs;i++) {
		sql_sarg = g_ptr_array_index(sql->sargs,i);
		if (sql_sarg->col_name) g_free(sql_sarg->col_name);
		if (sql_sarg->sarg) g_free(sql_sarg->sarg);
	}
	if (sql->sarg_tree) {
		mdb_sql_free_tree(sql->sarg_tree);
		sql->sarg_tree = NULL;
	}
	g_list_free(sql->sarg_stack);
	sql->sarg_stack = NULL;
	g_ptr_array_free(sql->columns,TRUE);
	g_ptr_array_free(sql->tables,TRUE);
	g_ptr_array_free(sql->sargs,TRUE);
	if (sql->mdb) {
		mdb_close(sql->mdb);	
		mdb_free_handle(sql->mdb);	
	}
}
void mdb_sql_reset(MdbSQL *sql)
{
int i;
MdbSQLColumn *c;
MdbSQLTable *t;
MdbSQLSarg *sql_sarg;

	if (sql->cur_table) {
		mdb_index_scan_free(sql->cur_table);
		mdb_free_tabledef(sql->cur_table);
		sql->cur_table = NULL;
	}
	if (sql->kludge_ttable_pg) {
		g_free(sql->kludge_ttable_pg);
		sql->kludge_ttable_pg = NULL;
	}
	for (i=0;i<sql->num_columns;i++) {
		c = g_ptr_array_index(sql->columns,i);
		mdb_sql_free_column(c);
	}
	for (i=0;i<sql->num_tables;i++) {
		t = g_ptr_array_index(sql->tables,i);
		mdb_sql_free_table(t);
	}
	for (i=0;i<sql->num_sargs;i++) {
		sql_sarg = g_ptr_array_index(sql->sargs,i);
		if (sql_sarg->col_name) g_free(sql_sarg->col_name);
		if (sql_sarg->sarg) g_free(sql_sarg->sarg);
	}
	if (sql->sarg_tree) {
		mdb_sql_free_tree(sql);
		sql->sarg_tree = NULL;
	}
	g_list_free(sql->sarg_stack);
	sql->sarg_stack = NULL;
	g_ptr_array_free(sql->columns,TRUE);
	g_ptr_array_free(sql->tables,TRUE);
	g_ptr_array_free(sql->sargs,TRUE);

	sql->all_columns = 0;
	sql->num_columns = 0;
	sql->columns = g_ptr_array_new();
	sql->num_tables = 0;
	sql->tables = g_ptr_array_new();
	sql->num_sargs = 0;
	sql->sargs = g_ptr_array_new();
}
static void print_break(int sz, int first)
{
int i;
	if (first) {
		fprintf(stdout,"+");
	}
	for (i=0;i<sz;i++) {
		fprintf(stdout,"-");
	}
	fprintf(stdout,"+");
}
static void print_value(char *v, int sz, int first)
{
int i;
int vlen;

	if (first) {
		fprintf(stdout,"|");
	}
	vlen = strlen(v);
	for (i=0;i<sz;i++) {
		fprintf(stdout,"%c",i >= vlen ? ' ' : v[i]);
	}
	fprintf(stdout,"|");
}
void mdb_sql_listtables(MdbSQL *sql)
{
	int i;
	MdbCatalogEntry *entry;
	MdbHandle *mdb = sql->mdb;
	MdbField fields[4];
	int num_fields = 0;
	char tmpstr[256];
	unsigned char row_buffer[4096];
	unsigned char *new_pg;
	int row_size;
	MdbTableDef *ttable, *table = NULL;
	MdbColumn *col, tcol;
	MdbSQLColumn *sqlcol;

	if (!mdb) {
		mdb_sql_error("You must connect to a database first");
		return;
	}
	mdb_read_catalog (mdb, MDB_TABLE);

	ttable = mdb_create_temp_table(mdb, "#listtables");

	memset(&tcol,0,sizeof(MdbColumn));
	strcpy(tcol.name, "Tables");
	tcol.col_size = 30;
	tcol.col_type = MDB_TEXT;
	tcol.is_fixed = 0;
	mdb_temp_table_add_col(ttable, &tcol);
	mdb_sql_add_column(sql, "Tables");
	sqlcol = g_ptr_array_index(sql->columns,0);
	sqlcol->disp_size = mdb_col_disp_size(&tcol);

	/* blank out the pg_buf */
	new_pg = mdb_new_data_pg(ttable->entry);
	memcpy(mdb->pg_buf, new_pg, mdb->fmt->pg_size);
	g_free(new_pg);

 	/* loop over each entry in the catalog */
 	for (i=0; i < mdb->num_catalog; i++) {
     		entry = g_ptr_array_index (mdb->catalog, i);
     		/* if it's a table */
     		if (entry->object_type == MDB_TABLE) {
       			if (strncmp (entry->object_name, "MSys", 4)) {
          			col = g_ptr_array_index(table->columns,0);
   				fields[0].value = entry->object_name;
   				fields[0].siz = strlen(entry->object_name); 
   				fields[0].is_fixed = 0;
   				fields[0].is_null = 0;
   				fields[0].start = 0;
   				fields[0].colnum = 0;

				row_size = mdb_pack_row(ttable, row_buffer, 1, &fields);
				mdb_add_row_to_pg(ttable,row_buffer, row_size);
				ttable->num_rows++;
			}
		}
	}
	sql->kludge_ttable_pg = g_memdup(mdb->pg_buf, mdb->fmt->pg_size);
	sql->cur_table = ttable;

}
void mdb_sql_describe_table(MdbSQL *sql)
{
	MdbTableDef *ttable, *table = NULL;
	MdbSQLTable *sql_tab;
	MdbCatalogEntry *entry;
	MdbHandle *mdb = sql->mdb;
	MdbColumn *col, tcol;
	MdbSQLColumn *sqlcol;
	int i;
	char colsize[11];
	MdbField fields[4];
	int num_fields = 0;
	char tmpstr[256];
	unsigned char row_buffer[4096];
	unsigned char *new_pg;
	int row_size;

	if (!mdb) {
		mdb_sql_error("You must connect to a database first");
		return;
	}

	sql_tab = g_ptr_array_index(sql->tables,0);

	mdb_read_catalog(mdb, MDB_TABLE);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type == MDB_TABLE &&
			!strcasecmp(entry->object_name,sql_tab->name)) {
				table = mdb_read_table(entry);
				break;
		}
	}
	if (!table) {
		mdb_sql_error("%s is not a table in this database", sql_tab->name);
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return;
	}

	mdb_read_columns(table);

	ttable = mdb_create_temp_table(mdb, "#describe");

	memset(&tcol,0,sizeof(MdbColumn));
	strcpy(tcol.name, "Column Name");
	tcol.col_size = 30;
	tcol.col_type = MDB_TEXT;
	tcol.is_fixed = 0;
	mdb_temp_table_add_col(ttable, &tcol);
	mdb_sql_add_column(sql, "Column Name");
	sqlcol = g_ptr_array_index(sql->columns,0);
	sqlcol->disp_size = mdb_col_disp_size(&tcol);

	memset(&tcol,0,sizeof(MdbColumn));
	strcpy(tcol.name, "Type");
	tcol.col_size = 20;
	tcol.col_type = MDB_TEXT;
	tcol.is_fixed = 0;
	mdb_temp_table_add_col(ttable, &tcol);
	mdb_sql_add_column(sql, "Type");
	sqlcol = g_ptr_array_index(sql->columns,1);
	sqlcol->disp_size = mdb_col_disp_size(&tcol);

	memset(&tcol,0,sizeof(MdbColumn));
	strcpy(tcol.name, "Size");
	tcol.col_size = 10;
	tcol.col_type = MDB_TEXT;
	tcol.is_fixed = 0;
	mdb_temp_table_add_col(ttable, &tcol);
	mdb_sql_add_column(sql, "Size");
	sqlcol = g_ptr_array_index(sql->columns,2);
	sqlcol->disp_size = mdb_col_disp_size(&tcol);

	/* blank out the pg_buf */
	new_pg = mdb_new_data_pg(ttable->entry);
	memcpy(mdb->pg_buf, new_pg, mdb->fmt->pg_size);
	g_free(new_pg);

     for (i=0;i<table->num_cols;i++) {

          col = g_ptr_array_index(table->columns,i);
   		fields[0].value = col->name; 
   		fields[0].siz = strlen(col->name); 
   		fields[0].is_fixed = 0;
   		fields[0].is_null = 0;
   		fields[0].start = 0;
   		fields[0].colnum = 0;

		strcpy(tmpstr, mdb_get_coltype_string(mdb->default_backend, col->col_type));
   		fields[1].value = tmpstr; 
   		fields[1].siz = strlen(tmpstr); 
   		fields[1].is_fixed = 0;
   		fields[1].is_null = 0;
   		fields[1].start = 0;
   		fields[1].colnum = 1;

		sprintf(colsize,"%d",col->col_size);
   		fields[2].value = colsize; 
   		fields[2].siz = strlen(colsize); 
   		fields[2].is_fixed = 0;
   		fields[2].is_null = 0;
   		fields[2].start = 0;
   		fields[2].colnum = 2;

		row_size = mdb_pack_row(ttable, row_buffer, 3, &fields);
		mdb_add_row_to_pg(ttable,row_buffer, row_size);
		ttable->num_rows++;
     }

	/* the column and table names are no good now */
	//mdb_sql_reset(sql);
	sql->kludge_ttable_pg = g_memdup(mdb->pg_buf, mdb->fmt->pg_size);
	sql->cur_table = ttable;
}

int mdb_sql_find_sargcol(MdbSargNode *node, gpointer data)
{
	MdbTableDef *table = data;
	int i;
	MdbColumn *col;

	if (!mdb_is_relational_op(node->op)) return 0;

	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		if (!strcasecmp(col->name, (char *)node->parent)) {
			node->col = col;
			break;
		}
	}
	return 0;
}
void 
mdb_sql_select(MdbSQL *sql)
{
int i,j;
MdbCatalogEntry *entry;
MdbHandle *mdb = sql->mdb;
MdbTableDef *table = NULL;
MdbSQLTable *sql_tab;
MdbColumn *col;
MdbSQLColumn *sqlcol;
int found = 0;

	if (!mdb) {
		mdb_sql_error("You must connect to a database first");
		return;
	}

	sql_tab = g_ptr_array_index(sql->tables,0);

	mdb_read_catalog(mdb, MDB_TABLE);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type == MDB_TABLE &&
			!strcasecmp(entry->object_name,sql_tab->name)) {
				table = mdb_read_table(entry);
				break;
		}
	}
	if (!table) {
		mdb_sql_error("%s is not a table in this database", sql_tab->name);
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return;
	}
	mdb_read_columns(table);
	mdb_read_indices(table);
	mdb_rewind_table(table);

	if (sql->all_columns) {
		for (i=0;i<table->num_cols;i++) {
			col=g_ptr_array_index(table->columns,i);
			sqlcol = mdb_sql_alloc_column();
			sqlcol->name = g_strdup(col->name);
			g_ptr_array_add(sql->columns, sqlcol);
			sql->num_columns++;
		}
	}
	/* verify all specified columns exist in this table */
	for (i=0;i<sql->num_columns;i++) {
		sqlcol = g_ptr_array_index(sql->columns,i);
		found=0;
		for (j=0;j<table->num_cols;j++) {
			col=g_ptr_array_index(table->columns,j);
			if (!strcasecmp(sqlcol->name, col->name)) {
				sqlcol->disp_size = mdb_col_disp_size(col);
				found=1;
				break;
			}
		}
		if (!found) {
			mdb_sql_error("Column %s not found",sqlcol->name);
			mdb_sql_reset(sql);
			return;
		}
	}

#if 0
	/* now add back the sargs */
	for (i=0;i<sql->num_sargs;i++) {
		sql_sarg=g_ptr_array_index(sql->sargs,i);
		//mdb_add_sarg_by_name(table,sql_sarg->col_name, sql_sarg->sarg);
	}
#endif
	/* 
	 * resolve column names to MdbColumn structs
	 */
	if (sql->sarg_tree) {
		mdb_sql_walk_tree(sql->sarg_tree, mdb_sql_find_sargcol, table);
		mdb_sql_walk_tree(sql->sarg_tree, mdb_find_indexable_sargs, NULL);
	}
	/* 
	 * move the sarg_tree.  
	 * XXX - this won't work when we implement joins 
	 */
	table->sarg_tree = sql->sarg_tree;
	sql->sarg_tree = NULL;

	sql->cur_table = table;
	mdb_index_scan_init(mdb, table);
}

void mdbsql_bind_column(MdbSQL *sql, int colnum, char *varaddr)
{
MdbTableDef *table = sql->cur_table;
MdbSQLColumn *sqlcol;
MdbColumn *col;
int j;

	/* sql columns are traditionally 1 based, so decrement colnum */
	sqlcol = g_ptr_array_index(sql->columns,colnum - 1);
	for (j=0;j<table->num_cols;j++) {
		col=g_ptr_array_index(table->columns,j);
		if (!strcasecmp(sqlcol->name, col->name)) {
			/* bind the column to its listed (SQL) position */
			mdb_bind_column(table, j+1, varaddr);
			break;
		}
	}
}
void mdbsql_bind_len(MdbSQL *sql, int colnum, int *len_ptr)
{
MdbTableDef *table = sql->cur_table;
MdbSQLColumn *sqlcol;
MdbColumn *col;
int j;

	/* sql columns are traditionally 1 based, so decrement colnum */
	sqlcol = g_ptr_array_index(sql->columns,colnum - 1);
	for (j=0;j<table->num_cols;j++) {
		col=g_ptr_array_index(table->columns,j);
		if (!strcasecmp(sqlcol->name, col->name)) {
			/* bind the column length to its listed (SQL) position */
			mdb_bind_len(table, j+1, len_ptr);
			break;
		}
	}
}
void mdbsql_bind_all(MdbSQL *sql)
{
int i;

	for (i=0;i<sql->num_columns;i++) {
		sql->bound_values[i] = (char *) malloc(MDB_BIND_SIZE);
		sql->bound_values[i][0] = '\0';
		mdbsql_bind_column(sql, i+1, sql->bound_values[i]);
	}
}
void mdbsql_dump_results(MdbSQL *sql)
{
int j;
MdbSQLColumn *sqlcol;

	/* print header */
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(sqlcol->disp_size, !j);
	}
	fprintf(stdout,"\n");
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_value(sqlcol->name,sqlcol->disp_size,!j);
	}
	fprintf(stdout,"\n");
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(sqlcol->disp_size, !j);
	}
	fprintf(stdout,"\n");

	/* print each row */
	while(mdb_fetch_row(sql->cur_table)) {
  		for (j=0;j<sql->num_columns;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			print_value(sql->bound_values[j],sqlcol->disp_size,!j);
		}
		fprintf(stdout,"\n");
	}

	/* footer */
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(sqlcol->disp_size, !j);
	}

	fprintf(stdout,"\n");
	/* clean up */
	for (j=0;j<sql->num_columns;j++) {
		if (sql->bound_values[j]) free(sql->bound_values[j]);
	}

	/* the column and table names are no good now */
	mdb_sql_reset(sql);
}

