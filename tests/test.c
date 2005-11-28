/* 
 * (C) 2005 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
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

#include <gtk/gtk.h>
#include <libgconf-bridge/gconf-bridge.h>

int
main (int argc, char **argv)
{
        GConfBridge *bridge;
        GtkWidget *window, *vbox, *check, *entry, *label, *scale, *tree_view;
        GtkAdjustment *adjustment;
        GtkListStore *store;
        GtkCellRenderer *cell;
        
        gtk_init (&argc, &argv);

        bridge = gconf_bridge_get ();
        gconf_client_add_dir (gconf_bridge_get_client (bridge),
                              "/apps/gconf-bridge-test",
                              GCONF_CLIENT_PRELOAD_RECURSIVE,
                              NULL);

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gconf_bridge_bind_window (bridge,
                                  "/apps/gconf-bridge-test/window",
                                  GTK_WINDOW (window), TRUE, TRUE);

        vbox = gtk_vbox_new (FALSE, 6);
        gtk_container_add (GTK_CONTAINER (window), vbox);

        check = gtk_check_button_new_with_label ("Check");
        gtk_box_pack_start_defaults (GTK_BOX (vbox), check);
        gconf_bridge_bind_property (bridge,
                                    "/apps/gconf-bridge-test/check",
                                    G_OBJECT (check), "active");

        entry = gtk_entry_new ();
        gtk_box_pack_start_defaults (GTK_BOX (vbox), entry);
        gconf_bridge_bind_property (bridge,
                                    "/apps/gconf-bridge-test/text",
                                    G_OBJECT (entry), "text");

        label = gtk_label_new ("Testing enums: Ellipsize mode");
        gtk_box_pack_start_defaults (GTK_BOX (vbox), label);
        gconf_bridge_bind_property (bridge,
                                    "/apps/gconf-bridge-test/ellipsize",
                                    G_OBJECT (label), "ellipsize");

        scale = gtk_hscale_new_with_range (0.0, 10.0, 0.01);
        gtk_box_pack_start_defaults (GTK_BOX (vbox), scale);
        adjustment = gtk_range_get_adjustment (GTK_RANGE (scale));
        gconf_bridge_bind_property_delayed (bridge,
                                            "/apps/gconf-bridge-test/scale",
                                            G_OBJECT (adjustment), "value");

        store = gtk_list_store_new (1, G_TYPE_STRING);
        tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
        gconf_bridge_bind_string_list_store (bridge,
                                             "/apps/gconf-bridge-test/list",
                                             store);
        g_object_unref (store);

        cell = gtk_cell_renderer_text_new ();
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
                                                     -1, "List",
                                                     cell, "text", 0, NULL);
        gtk_box_pack_start_defaults (GTK_BOX (vbox), tree_view);

        gtk_widget_show_all (window);

        gtk_main ();

        return 0;
}
