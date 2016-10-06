#include <stdio.h>
#include <gtk/gtk.h>
#include <pthread.h>


int Enter_Number(char *str)
{
	int len = strlen(str);
	int i = 0;
	int en = 0;
	for(i=0;i<len;i++)
	{
		if(*str == '\n')
			en ++;
	}

	return en;
}
GtkWidget *window;
char *End_Enter(char *str, int en_recv)
{
	int len = strlen(str);
	char *end = str + len;
	int en = 0;
	for(;en_recv!=-1 && end > str;end--)
	{
		if(*end =='\n')
			en_recv--;
	}
	end++;

	if(en_recv > -1)
		return str;
	else
		return end;
}
void *write_info(void *arg)
{
	GtkWidget *label = (GtkWidget*)arg;
	gchar* str = NULL;
	gchar  info[1024*16];
	int i =0;
	for(;i<100;i++)
	{
		str =  gtk_label_get_text( label ); 
		if(strlen(str) < 1024*15)
		{
			char *pos =	End_Enter(str, 20);
			sprintf(info, "%s\n%s %d", pos, "test", i);
		}
		else
			sprintf(info, "%s %d\n", "test", i);
		
		gtk_label_set_text( label,info );
		gtk_window_set_focus (window,label);
		sleep(1);
	}
}

static gboolean delete( GtkWidget *widget,
                        GtkWidget *event,
                        gpointer   data )
{
    gtk_main_quit ();
    return FALSE;
}

int main( int argc,
          char *argv[] )
{
	
	GtkWidget *fixed;
	GtkWidget *table;
	GtkWidget *notebook;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *checkbutton;
	pthread_t ppid;

	int i;
	char bufferf[32];
	char bufferl[32];
	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "Simple IP FireWall");


	g_signal_connect (G_OBJECT (window), "delete_event",
	              G_CALLBACK (delete), NULL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	/* Create a Fixed Container */
	fixed = gtk_fixed_new ();
	gtk_container_add (GTK_CONTAINER (window), fixed);
	gtk_widget_show (fixed);

	table = gtk_table_new (3, 6, FALSE);
	gtk_container_add (GTK_CONTAINER (fixed), table);

	


	/* Create a new notebook, place the position of the tabs */
	notebook = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
	gtk_table_attach_defaults (GTK_TABLE (table), notebook, 0, 6, 0, 1);
	gtk_widget_show (notebook);

	/* Let's append a bunch of pages to the notebook */
	for (i = 0; i < 2; i++) 
	{
		if(i==0)
		{
			sprintf(bufferf, "ToDo");
			sprintf(bufferl, "Filter Ruler");
		}
		else
		{
			sprintf(bufferf, "TODO");
			sprintf(bufferl, "Information");
		}
		frame = gtk_frame_new (bufferf);
		gtk_container_set_border_width (GTK_CONTAINER (frame), 5);
		gtk_widget_set_size_request (frame, 400, 425);
		gtk_widget_show (frame);

		label = gtk_label_new (bufferf);
		gtk_container_add (GTK_CONTAINER (frame), label);
		gtk_widget_show (label);
		if(i==1)
		{
			gtk_label_set_justify( label,GTK_JUSTIFY_LEFT );
			pthread_create(&ppid, NULL, write_info, (void*)label);
		}

		label = gtk_label_new (bufferl);
		gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);
	}

	/* Set what page to start at (page 4) */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 1);

	gtk_widget_show (table);
	gtk_widget_show (window);

	
	


	gtk_main ();
	return 0;
}


