#include "gmdb.h"

extern GtkWidget *app;
extern MdbHandle *mdb;

typedef struct GMdbDataWindow {
	gchar table_name[MDB_MAX_OBJ_NAME];
	GtkWidget *window;
} GMdbDataWindow;

static GList *window_list;

/* callbacks */
gint
gmdb_table_data_close(GtkWidget *w, GdkEvent *event, GMdbDataWindow *dataw)
{
	window_list = g_list_remove(window_list, dataw);
	g_free(dataw);
	return FALSE;
}
/* functions */
GtkWidget *
gmdb_table_data_new(MdbCatalogEntry *entry)
{
MdbTableDef *table;
MdbColumn *col;
GtkWidget *clist;
GtkWidget *scroll;
int i, rownum;
gchar *bound_data[256];
GMdbDataWindow *dataw = NULL;


	/* do we have an active window for this object? if so raise it */
	for (i=0;i<g_list_length(window_list);i++) {
		dataw = g_list_nth_data(window_list, i);
		if (!strcmp(dataw->table_name, entry->object_name)) {
			gdk_window_raise (dataw->window->window);
			return dataw->window;
		}
	}

	dataw = g_malloc(sizeof(GMdbDataWindow));
	strcpy(dataw->table_name, entry->object_name);

	dataw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);	
	gtk_window_set_title(GTK_WINDOW(dataw->window), entry->object_name);
	gtk_widget_set_usize(dataw->window, 300,200);
	gtk_widget_set_uposition(dataw->window, 50,50);
	gtk_widget_show(dataw->window);

    gtk_signal_connect (GTK_OBJECT (dataw->window), "delete_event",
        GTK_SIGNAL_FUNC (gmdb_table_data_close), dataw);


	scroll = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroll);
	gtk_container_add(GTK_CONTAINER(dataw->window), scroll);

	/* read table */
	table = mdb_read_table(entry);
	mdb_read_columns(table);
	mdb_rewind_table(table);

	clist = gtk_clist_new(table->num_cols);
	gtk_widget_show(clist);
	gtk_container_add(GTK_CONTAINER(scroll),clist);

	for (i=0;i<table->num_cols;i++) {
		/* bind columns */
		bound_data[i] = (char *) malloc(MDB_BIND_SIZE);
		bound_data[i][0] = '\0';
		mdb_bind_column(table, i+1, bound_data[i]);

		/* display column titles */
		col=g_ptr_array_index(table->columns,i);
		gtk_clist_set_column_title(GTK_CLIST(clist), i, col->name);
	}
	gtk_clist_column_titles_show(GTK_CLIST(clist));

	/* fetch those rows! */
	while(mdb_fetch_row(table)) {
		rownum = gtk_clist_append(GTK_CLIST(clist), bound_data);
	}

	/* free the memory used to bind */
	for (i=0;i<table->num_cols;i++) {
		free(bound_data[i]);
	}

	/* add this one to the window list */
	window_list = g_list_append(window_list, dataw);

	return dataw->window;
}
