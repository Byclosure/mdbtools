/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "gmdb.h"

extern GtkWidget *app;
extern MdbHandle *mdb;

GladeXML *schemawin_xml;
static gchar backend[100];
static gchar tabname[100];
static gchar file_path[256];
static gchar relation;
static gchar drops;

#define ALL_TABLES   "(All Tables)"

void
gmdb_schema_export()
{
FILE *outfile;
MdbTableDef *table;
MdbCatalogEntry *entry;
MdbColumn *col;
int i,k;
int need_headers = 0;
int need_quote = 0;
gchar delimiter[11];
gchar quotechar;
gchar lineterm[5];
gchar *str;
int rows=0;
char msg[100];
char            *the_relation;

	
	printf("file path %s\n",file_path);
	if ((outfile=fopen(file_path, "w"))==NULL) {
		gnome_warning_dialog("Unable to Open File!");
		return;
	}
	mdb_set_default_backend(mdb,backend);

	for (i=0; i < mdb->num_catalog; i++) {
     		entry = g_ptr_array_index (mdb->catalog, i);

     /* if it's a table */

     if (entry->object_type == MDB_TABLE) {
	 /* skip the MSys tables */
       if ((strlen(tabname) && !strcmp(entry->object_name,tabname)) ||
           (!strlen(tabname) && strncmp (entry->object_name, "MSys", 4))) {
	   
	   /* make sure it's a table (may be redundant) */

	   if (!strcmp (mdb_get_objtype_string (entry->object_type), "Table")) {
	       /* drop the table if it exists */
	       if (drops=='Y') 
		       fprintf(outfile, "DROP TABLE %s;\n", entry->object_name);

	       /* create the table */
	       fprintf (outfile, "CREATE TABLE %s\n", entry->object_name);
	       fprintf (outfile, " (\n");
	       	       
	       table = mdb_read_table (entry);

	       /* get the columns */
	       mdb_read_columns (table);

	       /* loop over the columns, dumping the names and types */

	       for (k = 0; k < table->num_cols; k++) {
		   col = g_ptr_array_index (table->columns, k);
		   
		   fprintf (outfile, "\t%s\t\t\t%s", col->name, 
			    mdb_get_coltype_string (mdb->default_backend, col->col_type));
		   
		   if (col->col_size != 0)
		     fprintf (outfile, " (%d)", col->col_size);
		   
		   if (k < table->num_cols - 1)
		     fprintf (outfile, ", \n");
		   else
		     fprintf (outfile, "\n");
		 }

	       fprintf (outfile, "\n);\n");
	       fprintf (outfile, "-- CREATE ANY INDEXES ...\n");
	       fprintf (outfile, "\n");
	     }
	 }
     }
   }
	fprintf (outfile, "\n\n");

	if (relation=='Y') {
		fprintf (outfile, "-- CREATE ANY Relationships ...\n");
		fprintf (outfile, "\n");
		while ((the_relation=mdb_get_relationships(mdb)) != NULL) {
			fprintf(outfile,"%s\n",the_relation);
			g_free(the_relation);
		}            
 	}

	fclose(outfile);
	sprintf(msg,"Schema exported successfully.\n");
	gnome_ok_dialog(msg);
}
void
gmdb_schema_export_cb(GtkWidget *w, gpointer data)
{
GtkWidget *schemawin, *combo, *checkbox, *entry;

	schemawin = glade_xml_get_widget (schemawin_xml, "schema_dialog");

	entry = glade_xml_get_widget (schemawin_xml, "filename_entry");
	strncpy(file_path,gtk_entry_get_text(GTK_ENTRY(entry)),255);
	combo = glade_xml_get_widget (schemawin_xml, "table_combo");
	strncpy(tabname,gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),99);
	if (!strcmp(tabname,ALL_TABLES)) tabname[0]='\0';
	combo = glade_xml_get_widget (schemawin_xml, "backend_combo");
	if (!strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),"Oracle")) strcpy(backend,"oracle");
	else if (!strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),"Sybase")) strcpy(backend,"sybase");
	else if (!strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),"MS SQL Server")) strcpy(backend,"sybase");
	else if (!strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),"PostgreSQL")) strcpy(backend,"postgres");
	else strcpy(backend,"access");
	checkbox = glade_xml_get_widget (schemawin_xml, "rel_checkbox");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)))
		relation = 'Y';
	else
		relation = 'N';
	checkbox = glade_xml_get_widget (schemawin_xml, "drop_checkbox");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)))
		drops = 'Y';
	else
		drops = 'N';
	//printf("%s %s %c\n",tabname,backend,relation);

	gtk_widget_destroy(schemawin);
	gmdb_schema_export();
}
void
gmdb_schema_help_cb(GtkWidget *w, gpointer data) 
{
	GError *error = NULL;

	gnome_help_display("gmdb.xml", "gmdb-schema", &error);
	if (error != NULL) {
		g_warning (error->message);
		g_error_free (error);
	}
}

void 
gmdb_schema_new_cb(GtkWidget *w, gpointer data) 
{
	GList *glist = NULL;
	GtkWidget *combo;
	MdbCatalogEntry *entry;
	int i;
   
	/* load the interface */
	schemawin_xml = glade_xml_new(GMDB_GLADEDIR "gmdb-schema.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(schemawin_xml);
	
	/* set signals with user data, anyone know how to do this in glade? */
	combo = glade_xml_get_widget (schemawin_xml, "table_combo");

	glist = g_list_append(glist, ALL_TABLES);
	/* loop over each entry in the catalog */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);

 		/* if it's a table */
		if (entry->object_type == MDB_TABLE) {
			/* skip the MSys tables */
			if (strncmp (entry->object_name, "MSys", 4)) {
				/* add table to list */
				glist = g_list_append(glist, entry->object_name);
	     		}
		} /* if MDB_TABLE */
	} /* for */
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);
}
