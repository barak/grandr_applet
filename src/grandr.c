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

// indent -kr -l100 -i4 -nut

#include <panel-applet-gconf.h>
#include <panel-applet.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <string.h>
#include <stdlib.h>

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>

#include "config.h"

typedef enum _name_kind {
    name_none = 0,
    name_string = (1 << 0),
    name_xid = (1 << 1),
    name_index = (1 << 2),
    name_preferred = (1 << 3),
} name_kind_t;

typedef struct {
    name_kind_t	    kind;
    char    	    *string;
    XID	    	    xid;
    int		    index;
} name_t;

typedef struct _crtc crtc_t;
typedef struct _output	output_t;
typedef struct _umode	umode_t;
typedef struct _output_prop output_prop_t;

struct _crtc {
    name_t	    crtc;
    Bool	    changing;
    XRRCrtcInfo	    *crtc_info;

    XRRModeInfo	    *mode_info;
    int		    x;
    int		    y;
    Rotation	    rotation;
    output_t	    **outputs;
    int		    noutput;
};

typedef struct {
    GtkWidget *applet;
    GtkWidget *image;

    GtkWidget *menu;
    GSList *size_menu_items;
    GSList *rotate_menu_items;

    gint size;
    GdkPixbuf *pixbuf;
    GdkPixbuf *scaled;

    /* XRandr Stuff */
    int major_version, minor_version;

    gboolean xr_lock_updates;

    XRRScreenConfiguration *xr_screen_conf;

    XRRScreenSize *xr_sizes;
    int xr_nsize;
    SizeID xr_current_size;

    Rotation xr_rotations;
    Rotation xr_current_rotation;
	
	XRRScreenResources  *res;
	
    /* Not yet Used  */

    //GtkWidget *scale;
    //GtkTooltips *tooltips;
    //PanelAppletOrient orientation;

} GrandrData;

static void
grandr_dialog_about(BonoboUIComponent * uic, GrandrData * grandr, const gchar * verbname);

static void grandr_applet_build_menu(GrandrData * grandr);

static char *Direction[33] = {
    "",
    N_(" Normal"),
    N_(" Left"),
    "",
    N_(" Inverted"),
    "",
    "",
    "",
    N_(" Right"),
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    N_(" Horizontal Reflection"), 
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    N_(" Vertical Reflection")
};


static const char Context_menu_xml[] =
    "<popup name=\"button3\">\n"
    "   <menuitem name=\"About Item\" "
    "             verb=\"VerbAbout\" "
    "           _label=\"About ...\"\n"
    "          pixtype=\"stock\" " "          pixname=\"gnome-stock-about\"/>\n" "</popup>\n";

static const BonoboUIVerb Context_menu_verbs[] = {
    BONOBO_UI_UNSAFE_VERB("VerbAbout", grandr_dialog_about),
    BONOBO_UI_VERB_END
};

BonoboGenericFactory *Applet_pointer;   // Pointer to the Applet

static output_t	*outputs = NULL;
static output_t	**outputs_tail = &outputs;
static crtc_t	*crtcs;
static umode_t	*umodes;
static int	num_crtcs;
static XRRScreenResources  *res;
static int screen = -1;


static void
grandr_dialog_about(BonoboUIComponent * uic, GrandrData * grandr, const gchar * verbname)
{

    static const gchar *authors[] = {
        "Matthew Allum <mallum@openedhand.com>",
        "Kevin DeKorte <kdekorte@gmail.com>",
        NULL
    };

    gchar *comment = g_strdup_printf(N_("Dynamically change display resolution and orientation.\nrandr protocol version %i.%i"),grandr->major_version,grandr->minor_version);
    

    gtk_show_about_dialog(NULL, "name", "Grandr",
                          "logo", grandr->pixbuf,
                          "authors", authors,
                          "copyright",
                          "Copyright \xc2\xa9 2003 OpenedHand Ltd., 2007 Kevin DeKorte", 
			  "comments", comment,
                          "version", VERSION, 
			  NULL);
    g_free(comment);
}

static void xrandr_get_config(GrandrData * data)
{
	screen = DefaultScreen(GDK_DISPLAY());
    data->xr_screen_conf = XRRGetScreenInfo(GDK_DISPLAY(), GDK_ROOT_WINDOW());

    data->xr_current_size = XRRConfigCurrentConfiguration(data->xr_screen_conf,
                                                          &data->xr_current_rotation);

    data->xr_sizes = XRRConfigSizes(data->xr_screen_conf, &data->xr_nsize);

    data->xr_rotations = XRRConfigRotations(data->xr_screen_conf, &data->xr_current_rotation);

    
}

gboolean xrandr_set_config(GrandrData * grandr)
{
    Status status = RRSetConfigFailed;

    if (grandr->xr_lock_updates)
        return FALSE;
	
    status = XRRSetScreenConfig(GDK_DISPLAY(),
                                grandr->xr_screen_conf,
                                GDK_ROOT_WINDOW(),
                                grandr->xr_current_size, grandr->xr_current_rotation, CurrentTime);

    return TRUE;

}

void menu_rotation_selected_cb(GtkMenuItem * menu_item, GrandrData * grandr)
{
    int rot = (Rotation) gtk_object_get_data(GTK_OBJECT(menu_item), "rotation_value");
    grandr->xr_current_rotation = rot;

    xrandr_set_config(grandr);
}

void menu_size_selected_cb(GtkMenuItem * menu_item, GrandrData * grandr)
{
    grandr->xr_current_size = (SizeID) gtk_object_get_data(GTK_OBJECT(menu_item), "size_index");
    xrandr_set_config(grandr);
}

void menu_output_selected(GtkMenuItem * menu_item, GrandrData * grandr)
{
	Status s;
	int i;
	gchar tmp_buf[128];
	int sel_output = (SizeID) gtk_object_get_data(GTK_OBJECT(menu_item), "output_index");
	grandr->res = XRRGetScreenResources(GDK_DISPLAY(), GDK_ROOT_WINDOW());
    
	i=sel_output;
		XRROutputInfo	*output_info = XRRGetOutputInfo (GDK_DISPLAY(), grandr->res, grandr->res->outputs[i]);
		if(output_info->crtc != None)
					g_snprintf(tmp_buf, 128, "/usr/bin/xrandr --output %s --off",
                			   output_info->name);
        else
        	g_snprintf(tmp_buf, 128, "/usr/bin/xrandr --output %s --auto",
                			   output_info->name);
		//fprintf(stderr,tmp_buf);
		g_spawn_command_line_async(tmp_buf,NULL);
		/*s = XRRSetCrtcConfig (GDK_DISPLAY(), grandr->res, output_info->crtc.xid, CurrentTime,
	//		      output_info->crtc->x, output_info->crtc->y, None, output_info->crtc,rotation,
		//	      rr_outputs, output_info->crtc->noutput);*/
    
//	
//	outputs[sel_output]->crtc_info = NULL;
	
}

GtkMenuPositionFunc applet_menu_position(GtkMenu * menu,
                          gint * x, gint * y, gboolean * push_in, gpointer * ptr_data)
{
    int xx, yy, rootx, rooty;
    GrandrData *data = (GrandrData *) ptr_data;

    gdk_window_get_root_origin(data->applet->window, &rootx, &rooty);
    gdk_window_get_origin(data->applet->window, &xx, &yy);
    if (rooty - data->menu->allocation.height < 0) {
        *y = rooty + data->applet->allocation.height;
    } else {
        *y = rooty - data->menu->allocation.height;
    }

    *x = xx + data->applet->allocation.x;

}

static gboolean
applet_button_release_event_cb(GtkWidget * widget, GdkEventButton * event, GrandrData * grandr)
{
    if (event->button == 1) {
        GSList *cur;

        xrandr_get_config(grandr);
        grandr_applet_build_menu(grandr);

        /* Icky way to resync active menu items with any external changes. */

        grandr->xr_lock_updates = TRUE; /* Needed as a simple signal block */

        cur = grandr->size_menu_items;

        while (cur != NULL) {
            if (grandr->xr_current_size ==
                (SizeID) gtk_object_get_data(GTK_OBJECT(cur->data), "size_index")) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(cur->data), TRUE);
                break;
            }
            cur = g_slist_next(cur);
        }

        cur = grandr->rotate_menu_items;

        while (cur != NULL) {
            if ((grandr->xr_current_rotation) == ((Rotation)
                                                  gtk_object_get_data(GTK_OBJECT(cur->data),
                                                                      "rotation_value"))) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(cur->data), TRUE);
                break;
            }
            cur = g_slist_next(cur);
        }

        grandr->xr_lock_updates = FALSE;

        gtk_widget_realize(grandr->menu);
        gtk_menu_popup(GTK_MENU(grandr->menu), NULL, NULL,
                       (GtkMenuPositionFunc)applet_menu_position,
                       grandr, event->button, event->time);
        return TRUE;
    }
    return FALSE;
}

static void applet_change_size_cb(PanelApplet * applet, gint size, GrandrData * grandr)
{
    if (grandr->size != size) {

        grandr->size = size;

        if (grandr->pixbuf) {
            if (grandr->scaled)
                g_object_unref(G_OBJECT(grandr->scaled));

            grandr->scaled = gdk_pixbuf_scale_simple(grandr->pixbuf, size, size,
                                                     GDK_INTERP_BILINEAR);

            gtk_image_set_from_pixbuf(GTK_IMAGE(grandr->image), grandr->scaled);

        }
    }
    gtk_widget_show(grandr->image);
}

static void grandr_applet_build_menu(GrandrData * grandr)
{
    GtkWidget *menu_item;
    GSList *group = NULL, *group_rotations = NULL;
    gint i,c;
    gchar tmp_buf[128];

/*    
    I know I have a memory leak here, but I need to clean up the old menu correctly 
    but this code causes the app to crash, which is not good
    if (grandr->menu != NULL) {
	gtk_widget_destroy(grandr->menu);
    }
*/
    grandr->menu = gtk_menu_new();
    gtk_menu_set_screen(GTK_MENU(grandr->menu), gtk_widget_get_screen(grandr->applet));

    grandr->xr_lock_updates = TRUE;

    /* Rotation menu entrys */

    if (grandr->xr_rotations > 1) {

        for (i = 1; i <= 32; i = i << 1) {


            if (grandr->xr_rotations) {
                menu_item = gtk_radio_menu_item_new_with_label(group_rotations, Direction[i]);
                group_rotations = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));

                gtk_object_set_data(GTK_OBJECT(menu_item), "rotation_value", (gpointer) i);

                if (i == grandr->xr_current_rotation) {
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
                }

                g_signal_connect(menu_item, "activate",
                                 G_CALLBACK(menu_rotation_selected_cb), grandr);

                gtk_menu_shell_append(GTK_MENU_SHELL(grandr->menu), menu_item);

                grandr->rotate_menu_items =
                    g_slist_append(grandr->rotate_menu_items, (gpointer) menu_item);

                gtk_widget_show(menu_item);
            }

        }
        menu_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(grandr->menu), menu_item);
        gtk_widget_show(menu_item);

    }
    /* Size menu entries */

    for (i = 0; i < grandr->xr_nsize; i++) {
        g_snprintf(tmp_buf, 128, "%5d x %-5d",
                   grandr->xr_sizes[i].width, grandr->xr_sizes[i].height);
        menu_item = gtk_radio_menu_item_new_with_label(group, tmp_buf);
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));

        if (i == grandr->xr_current_size)
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);

        gtk_object_set_data(GTK_OBJECT(menu_item), "size_index", (gpointer) i);

        g_signal_connect(menu_item, "activate", G_CALLBACK(menu_size_selected_cb), grandr);

        gtk_menu_shell_append(GTK_MENU_SHELL(grandr->menu), menu_item);

        grandr->size_menu_items = g_slist_append(grandr->size_menu_items, (gpointer) menu_item);

        gtk_widget_show(menu_item);
    }
        menu_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(grandr->menu), menu_item);
        gtk_widget_show(menu_item);
	/* Outputs menu entries */
	
//	XRRScreenResources  *res;
    grandr->res = XRRGetScreenResources(GDK_DISPLAY(), GDK_ROOT_WINDOW());
    
    for (i = 0; i < grandr->res->noutput; i++)
    {
		XRROutputInfo	*output_info = XRRGetOutputInfo (GDK_DISPLAY(), grandr->res, grandr->res->outputs[i]);
		g_snprintf(tmp_buf, 128, "%s ",
                   output_info->name);
		
        menu_item = gtk_check_menu_item_new_with_label( tmp_buf);
    
        gtk_object_set_data(GTK_OBJECT(menu_item), "output_index", (gpointer) i);
    
        if(output_info->connection>0)
			gtk_widget_set_sensitive (menu_item,FALSE);
		else
			if(output_info->crtc != None)	
	            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);	
		
        g_signal_connect(menu_item, "button-release-event", G_CALLBACK(menu_output_selected), grandr);
        gtk_menu_shell_append(GTK_MENU_SHELL(grandr->menu), menu_item);
		
        grandr->size_menu_items = g_slist_append(grandr->size_menu_items, (gpointer) menu_item);
        		
			
        gtk_widget_show(menu_item);
	
	
        
        
	}

    gtk_widget_show(grandr->menu);

    grandr->xr_lock_updates = FALSE;
}

static void grandr_applet_new(PanelApplet * applet)
{
    GrandrData *grandr;
    GtkIconTheme *icon_theme;

#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
#endif

    grandr = g_new0(GrandrData, 1);
    
    panel_applet_set_background_widget(applet, GTK_WIDGET(applet));

    /* Defaults */
    grandr->size = 0;
    grandr->menu = NULL;
    grandr->xr_screen_conf = NULL;
    XRRQueryVersion (GDK_DISPLAY(), &(grandr->major_version), &(grandr->minor_version));

    icon_theme = gtk_icon_theme_get_default();
    grandr->pixbuf = gtk_icon_theme_load_icon(icon_theme, "display", 64, 0, NULL);

    /* Applet Widgets  */

    grandr->image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(applet), grandr->image);

    grandr->applet = GTK_WIDGET(applet);

    grandr->xr_lock_updates = FALSE;
    xrandr_get_config(grandr);

    // grandr_applet_build_menu(grandr);


    /* Signals */
    g_signal_connect(grandr->applet, "button-release-event",
                     G_CALLBACK(applet_button_release_event_cb), grandr);

    g_signal_connect(grandr->applet, "change_size", G_CALLBACK(applet_change_size_cb), grandr);


    panel_applet_setup_menu(PANEL_APPLET(applet), Context_menu_xml, Context_menu_verbs, grandr);

    /* Make sure the panel image gets painted */
    applet_change_size_cb(applet, panel_applet_get_size(applet), grandr);

    gtk_widget_show_all(GTK_WIDGET(applet));
}


static gboolean grandr_applet_factory(PanelApplet * applet, const char *id, gpointer data)
{
    /* Check dpy has Xrandr */

    int event_basep, error_basep;

    if (XRRQueryExtension(GDK_DISPLAY(), &event_basep, &error_basep)) {
         // XXX should probably check for version too 
        grandr_applet_new(applet);
        return TRUE;
    }

    /* XXX below is depriciated but what replaces it ? */
    gnome_error_dialog("XServer lacks support to dynamic resizing.");

    return FALSE;
}

PANEL_APPLET_BONOBO_FACTORY("OAFIID:GrandrApplet_factory",
                            PANEL_TYPE_APPLET, "grandr", "0", grandr_applet_factory, NULL);
