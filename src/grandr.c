/* 
   GrandrApplet, A GNOME applet for quickly changing screen geometry.

   Copyright 2003 Matthew Allum <mallum@openedhand.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#include <panel-applet-gconf.h>
#include <panel-applet.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>

#include "config.h"

typedef struct 
{
  GtkWidget              *applet;
  GtkWidget              *frame;
  GtkWidget              *image;
  
  GtkWidget              *menu;
  GSList                 *size_menu_items;
  GSList                 *rotate_menu_items;  

  gint                    size;

  /* XRandr Stuff */

  gboolean                xr_lock_updates;

  XRRScreenConfiguration *xr_screen_conf;

  XRRScreenSize          *xr_sizes;
  int                     xr_nsize;
  SizeID	          xr_current_size;

  Rotation                xr_rotations;
  Rotation                xr_current_rotation;

  /* Not yet Used  */

  GtkWidget              *scale;
  GtkTooltips	         *tooltips;
  PanelAppletOrient       orientation;

} GrandrData; 

static void 
grandr_dialog_about (BonoboUIComponent *uic,
		     GrandrData        *grandr,
		     const gchar       *verbname );

static void
grandr_applet_build_menu (GrandrData  *grandr);


static char *Direction[5] = 
  {
    " Normal", 
    " Left", 
    " Inverted", 
    " Right",
    "\n"
  };

static const char Context_menu_xml [] =					       
  "<popup name=\"button3\">\n"						
  "   <menuitem name=\"About Item\" "
  "             verb=\"VerbAbout\" "
  "           _label=\"About ...\"\n" 
  "          pixtype=\"stock\" "
  "          pixname=\"gnome-stock-about\"/>\n"  
  "</popup>\n";

static const BonoboUIVerb Context_menu_verbs [] = {
  BONOBO_UI_UNSAFE_VERB ("VerbAbout", grandr_dialog_about ),
  BONOBO_UI_VERB_END  						
};

BonoboGenericFactory *Applet_pointer;	// Pointer to the Applet

static void 
grandr_dialog_about (BonoboUIComponent *uic,
		     GrandrData        *grandr,
		     const gchar       *verbname )
{
  static GtkWidget *about = NULL;
  GdkPixbuf        *pixbuf = NULL;
  gchar            *file;

  static const gchar *authors[] =
    {
      "Matthew Allum <mallum@openedhand.com>",
      NULL
    };
    
  const char *documenters [] = {
    NULL
  };
  
  const char *translator_credits = NULL;

  if (about) 
    {
      gtk_window_set_screen (GTK_WINDOW (about),
			     gtk_widget_get_screen (grandr->applet));
      gtk_widget_show (about);
      gtk_window_present (GTK_WINDOW (about));
      return;
    }

  /*
  file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
				    "gnome-windows.png", TRUE, NULL);
  pixbuf = gdk_pixbuf_new_from_file (file, NULL);
  g_free(file);
  */

  about = gnome_about_new ("Grandr", VERSION,
			   "Copyright \xc2\xa9 2003 OpenedHand Ltd.",
			   "Dynamically change display resolution and orientation.",
			   authors,
			   documenters,
			   translator_credits,
			   pixbuf );

  gtk_window_set_screen (GTK_WINDOW (about),
			 gtk_widget_get_screen (grandr->applet));

  if (pixbuf) {
    gtk_window_set_icon (GTK_WINDOW (about), pixbuf);
    g_object_unref (pixbuf);
  }

  g_signal_connect (G_OBJECT(about), "destroy",
		    (GCallback)gtk_widget_destroyed, &about);

  gtk_widget_show (about);
}

static void
xrandr_get_config ( GrandrData *data )
{
  data->xr_screen_conf = XRRGetScreenInfo (GDK_DISPLAY(), GDK_ROOT_WINDOW());
  
  data->xr_current_size = XRRConfigCurrentConfiguration (data->xr_screen_conf, 
							 &data->xr_current_rotation);

  data->xr_sizes = XRRConfigSizes(data->xr_screen_conf, &data->xr_nsize);
  
  data->xr_rotations = XRRConfigRotations(data->xr_screen_conf,
					  &data->xr_current_rotation);
}

gboolean
xrandr_set_config( GrandrData  *grandr )
{
  Status  status = RRSetConfigFailed;

  if (grandr->xr_lock_updates) return FALSE;
  
  status = XRRSetScreenConfig (GDK_DISPLAY(), 
			       grandr->xr_screen_conf, 
			       GDK_ROOT_WINDOW(), 
			       grandr->xr_current_size, 
			       grandr->xr_current_rotation, 
			       CurrentTime);

  return TRUE;

}

void 
menu_rotation_selected_cb (GtkMenuItem *menu_item,
			   GrandrData  *grandr )
{
  int rot = (Rotation)gtk_object_get_data(GTK_OBJECT(menu_item), "rotation_value");

  grandr->xr_current_rotation = 1 << rot;
    
  xrandr_set_config( grandr );
}

void 
menu_size_selected_cb (GtkMenuItem *menu_item,
		       GrandrData  *grandr )
{
  grandr->xr_current_size = (SizeID)gtk_object_get_data(GTK_OBJECT(menu_item), 
							"size_index");
  xrandr_set_config( grandr );
}


void 				/* XXX Currently unused */
applet_menu_position (GtkMenu     *menu,
		      gint        *x,
		      gint        *y,
		      gboolean    *push_in,
		      gpointer    *ptr_data )
{
  int xx, yy, rootx, rooty;
  GrandrData  *data = (GrandrData  *)ptr_data;

  gdk_window_get_root_origin(data->applet->window, &rootx, &rooty);
  gdk_window_get_origin(data->applet->window, &xx, &yy);

  *y = rooty - data->menu->allocation.height;
  *x = xx    + data->applet->allocation.x;

}

static gboolean
applet_button_release_event_cb (GtkWidget      *widget, 
				GdkEventButton *event, 
				GrandrData      *grandr   )
{
  if (event->button == 1) 
    {
      GSList* cur;

      xrandr_get_config ( grandr );

      /* Icky way to resync active menu items with any external changes.*/

      grandr->xr_lock_updates = TRUE; /* Needed as a simple signal block */

      cur = grandr->size_menu_items;

      while (cur != NULL)
	{
	  if (grandr->xr_current_size == (SizeID) gtk_object_get_data(GTK_OBJECT(cur->data), "size_index"))
	    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (cur->data), 
					    TRUE);
	  cur = g_slist_next(cur);
	}

      cur = grandr->rotate_menu_items;

      while (cur != NULL)
	{
	  if ((1 << grandr->xr_current_rotation) == ((Rotation) gtk_object_get_data(GTK_OBJECT(cur->data), "rotation_value") & 0xf ))
	    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (cur->data), 
					    TRUE);
	  cur = g_slist_next(cur);
	}

      grandr->xr_lock_updates = FALSE;


      gtk_menu_popup (GTK_MENU(grandr->menu), 
		      NULL, NULL, 
		      /* applet_menu_position */ NULL, 
		      grandr, 
		      event->button,
		      event->time);
      return TRUE;
    }
  return FALSE;
}

static void
applet_change_size_cb ( PanelApplet *applet, 
                        gint         size, 
                        GrandrData  *grandr )
{
  if (grandr->size != size) 
    {
      GdkPixbuf *pixbuf;
      GdkPixbuf *scaled;

      grandr->size = size;

      pixbuf = gdk_pixbuf_new_from_file (DATADIR "pixmaps/grandr.png", 
					 NULL);
      if (pixbuf) 
	{
	  scaled = gdk_pixbuf_scale_simple (pixbuf, size, size, 
					    GDK_INTERP_BILINEAR);

	  gtk_image_set_from_pixbuf (GTK_IMAGE (grandr->image), scaled);
	  gtk_widget_show (grandr->image);

	  g_object_unref (G_OBJECT (pixbuf));
	}
    }
}

static void
grandr_applet_build_menu (GrandrData  *grandr)
{
  GtkWidget    *menu_item;
  GSList        *group = NULL, *group_rotations = NULL;
  gint          i;
  gchar         tmp_buf[128];

  grandr->menu = gtk_menu_new();
  gtk_menu_set_screen (GTK_MENU (grandr->menu), 
		       gtk_widget_get_screen (grandr->applet));

  grandr->xr_lock_updates = TRUE;

  /* Size menu entries */

  for (i = 0; i < grandr->xr_nsize; i++)
    {
      g_snprintf(tmp_buf, 128, "%5d x %-5d", 
		 grandr->xr_sizes[i].width, grandr->xr_sizes[i].height );
      menu_item = gtk_radio_menu_item_new_with_label (group, tmp_buf);
      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));

      if (i == grandr->xr_current_size)
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), TRUE);

      gtk_object_set_data(GTK_OBJECT (menu_item), "size_index", (gpointer)i );

      g_signal_connect (menu_item, "activate",
			G_CALLBACK (menu_size_selected_cb), grandr );

      gtk_menu_shell_append (GTK_MENU_SHELL (grandr->menu), menu_item);

      grandr->size_menu_items = g_slist_append (grandr->size_menu_items, (gpointer)menu_item );

      gtk_widget_show (menu_item);
    }

  /* Rotation menu entrys */
  
  if (grandr->xr_rotations > 1)
    {
      menu_item = gtk_separator_menu_item_new  ();
      gtk_menu_shell_prepend (GTK_MENU_SHELL(grandr->menu), menu_item);
      gtk_widget_show (menu_item);

      for (i = 0; i < 4; i ++) 
	{
	  if ((grandr->xr_rotations >> i) & 1)  
	    {
	      menu_item = gtk_radio_menu_item_new_with_label (group_rotations,
							      Direction[i]);
	      group_rotations = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));

	      gtk_object_set_data(GTK_OBJECT (menu_item), "rotation_value", 
				  (gpointer)i /*(grandr->xr_rotations >> i)*/ );

	      if ((1 << i) == grandr->xr_current_rotation & 0xf)
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), TRUE);

	      g_signal_connect (menu_item, "activate",
				G_CALLBACK (menu_rotation_selected_cb), 
				grandr );

	      gtk_menu_shell_prepend (GTK_MENU_SHELL(grandr->menu),
				     menu_item);

	      grandr->rotate_menu_items = g_slist_append (grandr->rotate_menu_items, (gpointer)menu_item );

	      gtk_widget_show (menu_item);
	    }
	}
    }

  gtk_widget_show (grandr->menu);

  grandr->xr_lock_updates = FALSE;
} 

static void 
grandr_applet_new ( PanelApplet *applet)
{
  GrandrData   *grandr;



  grandr = g_new0 (GrandrData, 1);

  /* Defaults */
  grandr->size = 0;
  grandr->menu = NULL;
  grandr->xr_screen_conf = NULL;

  /* Applet Widgets  */
  grandr->frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (grandr->frame), GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (applet), grandr->frame);
	
  grandr->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (grandr->frame), grandr->image);

  grandr->applet = GTK_WIDGET (applet);

  grandr->xr_lock_updates = FALSE;
  xrandr_get_config ( grandr );
  
  grandr_applet_build_menu (grandr);


  /* Signals */
  g_signal_connect (grandr->applet, "button-release-event",
		    G_CALLBACK (applet_button_release_event_cb), grandr );

  g_signal_connect (grandr->applet, "change_size",
		    G_CALLBACK (applet_change_size_cb), grandr );


  panel_applet_setup_menu ( PANEL_APPLET (applet),	
			    Context_menu_xml,         
			    Context_menu_verbs,       
			    grandr );                  

  /* Make sure the panel image gets painted */
  applet_change_size_cb ( applet, panel_applet_get_size (applet), grandr );

  gtk_widget_show_all(GTK_WIDGET(applet));
}

static gboolean
grandr_applet_factory ( PanelApplet  *applet, 
                        const char   *id, 
                        gpointer      data )
{
  /* Check dpy has Xrandr */

  int event_basep, error_basep;

  if (XRRQueryExtension (GDK_DISPLAY(), &event_basep, &error_basep))
    {
      /* XXX should probably check for version too 
        int major_version, minor_version;
	XRRQueryVersion (GDK_DISPLAY(), &major_version, &minor_version);
      */
	grandr_applet_new(applet);		
	return TRUE;
    }

  /* XXX below is depriciated but what replaces it ? */
  gnome_error_dialog ("XServer lacks support to dynamic resizing.");

  return FALSE;
}

PANEL_APPLET_BONOBO_FACTORY ( "OAFIID:GrandrApplet_factory", 
			      PANEL_TYPE_APPLET, 
			      "grandr",
			      "0",
			      grandr_applet_factory,
			      NULL ) ;

