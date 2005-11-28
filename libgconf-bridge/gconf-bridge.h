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

#ifndef __GCONF_BRIDGE_H__
#define __GCONF_BRIDGE_H__

#include <gconf/gconf-client.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkliststore.h>

G_BEGIN_DECLS

void gconf_bridge_install_default_error_handler (void);

typedef struct _GConfBridge GConfBridge;

GConfBridge *gconf_bridge_get                    (void);

GConfClient *gconf_bridge_get_client             (GConfBridge  *bridge);

guint        gconf_bridge_bind_property_full     (GConfBridge  *bridge,
                                                  const char   *key,
                                                  GObject      *object,
                                                  const char   *prop,
                                                  gboolean      delayed_sync);

#define gconf_bridge_bind_property(bridge, key, object, prop) \
        gconf_bridge_bind_property_full ((bridge), (key), \
                                         (object), (prop), FALSE)

#define gconf_bridge_bind_property_delayed(bridge, key, object, prop) \
        gconf_bridge_bind_property_full ((bridge), (key), \
                                         (object), (prop), TRUE)

guint        gconf_bridge_bind_window            (GConfBridge  *bridge,
                                                  const char   *key_prefix,
                                                  GtkWindow    *window,
                                                  gboolean      bind_size,
                                                  gboolean      bind_pos);

#define gconf_bridge_bind_window_size(bridge, key_prefix, window) \
        gconf_bridge_bind_window ((bridge), (key_prefix), (window), TRUE, FALSE)

#define gconf_bridge_bind_window_pos(bridge, key_prefix, window) \
        gconf_bridge_bind_window ((bridge), (key_prefix), (window), FALSE, TRUE)

guint        gconf_bridge_bind_string_list_store (GConfBridge  *bridge,
                                                  const char   *key,
                                                  GtkListStore *list_store);

void         gconf_bridge_unbind                 (GConfBridge  *bridge,
                                                  guint         binding_id);

G_END_DECLS

#endif /* __GCONF_BRIDGE_H__ */
