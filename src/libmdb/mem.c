/* MDB Tools - A library for reading MS Access database files
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

#include "mdbtools.h"
#include <locale.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/**
 * mdb_init:
 *
 * Initializes the LibMDB library.  This function should be called exactly once
 * by calling program and prior to any other function.
 *
 **/
void mdb_init()
{
	mdb_init_backends();
}

/**
 * mdb_exit:
 *
 * Cleans up the LibMDB library.  This function should be called exactly once
 * by the calling program prior to exiting (or prior to final use of LibMDB 
 * functions).
 *
 **/
void mdb_exit()
{
	mdb_remove_backends();
}

/* private function */
MdbStatistics *mdb_alloc_stats(MdbHandle *mdb)
{
	mdb->stats = g_malloc0(sizeof(MdbStatistics));
	return mdb->stats;
/* private function */
}
/* private function */
void 
mdb_free_stats(MdbHandle *mdb)
{
	g_free(mdb->stats);
	mdb->stats = NULL;
}

MdbFile *
mdb_alloc_file()
{
	MdbFile *f;

	f = (MdbFile *) malloc(sizeof(MdbFile));
	memset(f, '\0', sizeof(MdbFile));

	return f;	
}
void 
mdb_free_file(MdbFile *f)
{
	if (!f) return;	
	if (f->fd) close(f->fd);
	if (f->filename) free(f->filename);
	free(f);
	f = NULL;
}

MdbHandle *mdb_alloc_handle()
{
MdbHandle *mdb;

	mdb = (MdbHandle *) malloc(sizeof(MdbHandle));
	memset(mdb, '\0', sizeof(MdbHandle));
	mdb_set_default_backend(mdb, "access");

	return mdb;	
}
void mdb_free_handle(MdbHandle *mdb)
{
	if (!mdb) return;	

	if (mdb->stats) mdb_free_stats(mdb);
	if (mdb->catalog) mdb_free_catalog(mdb);
	if (mdb->f && mdb->f->refs<=0) mdb_free_file(mdb->f);
	if (mdb->backend_name) free(mdb->backend_name);
	free(mdb);
	mdb = NULL;
}

void mdb_alloc_catalog(MdbHandle *mdb)
{
	mdb->catalog = g_ptr_array_new();
}
void mdb_free_catalog(MdbHandle *mdb)
{
	unsigned int i;
	MdbCatalogEntry *entry;
	for (i=0; i<mdb->catalog->len; i++) {
		entry = g_ptr_array_index(mdb->catalog, i);
		g_free (entry);
	}
	g_ptr_array_free(mdb->catalog, TRUE);
	mdb->catalog = NULL;
}

MdbTableDef *mdb_alloc_tabledef(MdbCatalogEntry *entry)
{
MdbTableDef *table;

	table = (MdbTableDef *) malloc(sizeof(MdbTableDef));
	memset(table, '\0', sizeof(MdbTableDef));
	table->entry=entry;
	strcpy(table->name, entry->object_name);

	return table;	
}
void 
mdb_free_tabledef(MdbTableDef *table)
{
	if (!table) return;
	if (table->usage_map) free(table->usage_map);
	if (table->free_usage_map) free(table->free_usage_map);
	free(table);
	table = NULL;
}

void
mdb_append_column(GPtrArray *columns, MdbColumn *in_col)
{
MdbColumn *col;
		
 	col = g_memdup(in_col,sizeof(MdbColumn));
	g_ptr_array_add(columns, col);
}
void
mdb_free_columns(GPtrArray *columns)
{
	g_ptr_array_free(columns, TRUE);
	columns = NULL;
}

void 
mdb_append_index(GPtrArray *indices, MdbIndex *in_idx)
{
MdbIndex *idx;
		
 	idx = g_memdup(in_idx,sizeof(MdbIndex));
	g_ptr_array_add(indices, idx);
}
void
mdb_free_indices(GPtrArray *indices)
{
	g_ptr_array_free(indices, TRUE);
	indices = NULL;
}
