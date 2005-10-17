/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
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

#undef MDB_DEBUG
//#define MDB_DEBUG 1

void dump_kkd(MdbHandle *mdb, void *kkd, size_t len);

int
main(int argc, char **argv)
{
	MdbHandle *mdb;
	MdbTableDef *table;
	gchar name[256];
	void *buf;
	int col_num;
	int found = 0;

	if (argc < 4) {
		fprintf(stderr,"Usage: %s <file> <object name> <prop col>\n",
			argv[0]);
		return 1;
	}

	mdb_init();

	mdb = mdb_open(argv[1], MDB_NOFLAGS);
	if (!mdb) {
		mdb_exit();
		return 1;
	}

	table = mdb_read_table_by_name(mdb, "MSysObjects", MDB_ANY);
	if (!table) {
		mdb_close(mdb);
		mdb_exit();
		return 1;
	}
	mdb_read_columns(table);
	mdb_rewind_table(table);

	mdb_bind_column_by_name(table, "Name", name, NULL);
	buf = g_malloc(MDB_BIND_SIZE);
	col_num = mdb_bind_column_by_name(table, argv[3], buf, NULL);
	if (col_num < 1) {
		g_free(buf);
		mdb_free_tabledef(table);
		mdb_close(mdb);
		mdb_exit();
		return 1;
	}

	while(mdb_fetch_row(table)) {
		if (!strcmp(name, argv[2])) {
			found = 1;
			break;
		}
	}

	if (found) {
		MdbColumn *col;
		gchar kkd_ptr[MDB_MEMO_OVERHEAD];
		void *kkd_pg = g_malloc(200000);
		size_t len, pos;

		memcpy(kkd_ptr, buf, MDB_MEMO_OVERHEAD);
		col = g_ptr_array_index(table->columns, col_num - 1);
		len = mdb_ole_read(mdb, col, kkd_ptr, MDB_BIND_SIZE);
		memcpy(kkd_pg, buf, len);
		pos = len;
		while ((len = mdb_ole_read_next(mdb, col, kkd_ptr))) {
			memcpy(kkd_pg + pos, buf, len);
			pos += len;
		}
		dump_kkd(mdb, kkd_pg, pos);
		g_free(kkd_pg);
	}

	g_free(buf);
	mdb_free_tabledef(table);
	mdb_close(mdb);
	mdb_exit();

	return 0;
}
void print_keyvalue(gpointer key, gpointer value, gpointer user_data)
{
		printf("%s = %s\n", (gchar *)key, (gchar *)value);
}
void dump_kkd(MdbHandle *mdb, void *kkd, size_t len)
{
	guint32 record_len;
	guint16 record_type;
	size_t pos;
	GPtrArray *names = NULL;
	MdbProperties *props;

#ifdef MDB_DEBUG
	buffer_dump(kkd, 0, len);
#endif
	if (strcmp("KKD", kkd)) {
		fprintf(stderr, "Unrecognized format.\n");
		return;
	}
	

	pos = 4;
	while (pos < len) {
		record_len = mdb_get_int32(kkd, pos);
		record_type = mdb_get_int16(kkd, pos + 4);
		//printf("len = %d type = %d\n", record_len, record_type);
		switch (record_type) {
			case 0x80:
				names = mdb_read_props_list(kkd+pos+6, record_len - 6);
				break;
			case 0x00:
				if (!names) {
					printf("sequence error!\n");
					break;
				}
				props = mdb_read_props(mdb, names, kkd+pos+6, record_len - 6);
				printf("type 0x00 name %s\n", props->name ? props->name : "(none)");
				g_hash_table_foreach(props->hash, print_keyvalue, NULL);
				mdb_free_props(props);
				break;
			case 0x01:
				if (!names) {
					printf("sequence error!\n");
					break;
				}
				props = mdb_read_props(mdb, names, kkd+pos+6, record_len - 6);
				printf("type 0x01 name %s\n", props->name ? props->name : "(none)");
				g_hash_table_foreach(props->hash, print_keyvalue, NULL);
				mdb_free_props(props);
				break;
			default:
				fprintf(stderr,"Unknown record type %d\n", record_type);
				return;
		}
		pos += record_len;
	}
}

