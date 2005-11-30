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

#include <glib/gi18n-lib.h>
#include <gtk/gtkmessagedialog.h>
#include <string.h>

#include "gconf-bridge.h"

struct _GConfBridge {
        GConfClient *client;
        
        GHashTable *bindings;
};

/* The data structures for the different kinds of bindings */
typedef enum {
        BINDING_PROP,
        BINDING_WINDOW,
        BINDING_LIST_STORE
} BindingType;

typedef struct {
        BindingType type;
        guint id;
        gboolean delayed_mode;

        char *key;
        guint val_notify_id;

        GObject *object;
        GParamSpec *prop;
        gulong prop_notify_id;

        guint sync_timeout_id; /* Used in delayed mode */

        gboolean blocked;
} PropBinding;

typedef struct {
        BindingType type;
        guint id;

        char *width_key;
        char *height_key;
        char *x_key;
        char *y_key;

        GtkWindow *window;
        gulong configure_event_id;
        gulong unmap_id;
        guint sync_timeout_id;
} WindowBinding;

typedef struct {
        BindingType type;
        guint id;

        char *key;
        guint val_notify_id;

        GtkListStore *list_store;
        guint row_inserted_id;
        guint row_changed_id;
        guint row_deleted_id;
        guint rows_reordered_id;
        guint sync_idle_id;

        gboolean blocked;
} ListStoreBinding;

/* Some trickery to be able to treat the data structures generically */
typedef union {
        BindingType      type;

        PropBinding      prop_binding;
        WindowBinding    window_binding;
        ListStoreBinding list_store_binding;
} Binding;

/* Function prototypes */
static void
unbind (Binding *binding);

static GConfBridge *bridge = NULL; /* Global GConfBridge object */

/* Free up all resources allocated by the GConfBridge. Called on exit. */
static void
destroy_bridge (void)
{
        g_hash_table_destroy (bridge->bindings);
        g_object_unref (bridge->client);

        g_free (bridge);
}

/**
 * gconf_bridge_get
 *
 * Returns the #GConfBridge. This is a singleton object.
 *
 * Return value: The #GConfBridge.
 **/
GConfBridge *
gconf_bridge_get (void)
{
        if (bridge)
                return bridge;

        gconf_bridge_install_default_error_handler ();

        bridge = g_new (GConfBridge, 1);

        bridge->client = gconf_client_get_default ();
        bridge->bindings = g_hash_table_new_full (NULL, NULL, NULL,
                                                  (GDestroyNotify) unbind);

        g_atexit (destroy_bridge);

        return bridge;
}

/**
 * gconf_bridge_get_client
 * @bridge: A #GConfBridge
 *
 * Returns the #GConfClient used by @bridge. This is the same #GConfClient 
 * as returned by gconf_client_get_default().
 *
 * Return value: A #GConfClient.
 **/
GConfClient *
gconf_bridge_get_client (GConfBridge *bridge)
{
        g_return_val_if_fail (bridge != NULL, NULL);

        return bridge->client;
}

/* Generate an ID for a new binding */
static guint
new_id (void)
{
        static guint id_counter = 0;

        id_counter++;

        return id_counter;
}

/*
 * Property bindings
 */

/* Syncs a value from GConf to an object property */
static void
prop_binding_sync_pref_to_prop (PropBinding *binding,
                                GConfValue  *pref_value)
{
        GValue src_value, value;
        
        memset (&src_value, 0, sizeof (GValue));

        /* First, convert GConfValue to GValue */
        switch (pref_value->type) {
        case GCONF_VALUE_STRING:
                g_value_init (&src_value, G_TYPE_STRING);
                g_value_set_string (&src_value,
                                    gconf_value_get_string (pref_value));
                break;
        case GCONF_VALUE_INT:
                g_value_init (&src_value, G_TYPE_INT);
                g_value_set_int (&src_value,
                                 gconf_value_get_int (pref_value));
                break;
        case GCONF_VALUE_BOOL:
                g_value_init (&src_value, G_TYPE_BOOLEAN);
                g_value_set_boolean (&src_value,
                                     gconf_value_get_bool (pref_value));
                break;
        case GCONF_VALUE_FLOAT:
                g_value_init (&src_value, G_TYPE_FLOAT);
                g_value_set_float (&src_value,
                                   gconf_value_get_float (pref_value));
                break;
        default:
                g_warning ("prop_binding_sync_pref_to_prop: Unhandled value "
                           "type '%d'.\n", pref_value->type);

                return;
        }

        /* Then convert to the type expected by the object, if necessary */
        memset (&value, 0, sizeof (GValue));
        g_value_init (&value,
                      G_PARAM_SPEC_VALUE_TYPE (binding->prop));

        if (src_value.g_type != value.g_type) {
                if (!g_value_transform (&src_value, &value)) {
                        g_warning ("prop_binding_sync_pref_to_prop: Failed to "
                                   "transform a \"%s\" to a \"%s\".",
                                   g_type_name (src_value.g_type),
                                   g_type_name (value.g_type));
                        
                        goto done;
                }

                g_object_set_property (binding->object,
                                       binding->prop->name, &value);
        } else {
                g_object_set_property (binding->object,
                                       binding->prop->name, &src_value);
        }
        
done:
        g_value_unset (&src_value);
        g_value_unset (&value);
}

/* Syncs an object property to GConf */
static void
prop_binding_sync_prop_to_pref (PropBinding *binding)
{
        GValue value;

        memset (&value, 0, sizeof (GValue));

        g_value_init (&value,
                      G_PARAM_SPEC_VALUE_TYPE (binding->prop));
        g_object_get_property (binding->object,
                               binding->prop->name,
                               &value);

        switch (value.g_type) {
        case G_TYPE_STRING:
                gconf_client_set_string (bridge->client, binding->key,
                                         g_value_get_string (&value), NULL);
                break;
        case G_TYPE_INT:
                gconf_client_set_int (bridge->client, binding->key,
                                      g_value_get_int (&value), NULL);
                break;
        case G_TYPE_UINT:
                gconf_client_set_int (bridge->client, binding->key,
                                      g_value_get_uint (&value), NULL);
                break;
        case G_TYPE_LONG:
                gconf_client_set_int (bridge->client, binding->key,
                                      g_value_get_long (&value), NULL);
                break;
        case G_TYPE_ULONG:
                gconf_client_set_int (bridge->client, binding->key,
                                      g_value_get_ulong (&value), NULL);
                break;
        case G_TYPE_INT64:
                gconf_client_set_int (bridge->client, binding->key,
                                      g_value_get_int64 (&value), NULL);
                break;
        case G_TYPE_UINT64:
                gconf_client_set_int (bridge->client, binding->key,
                                      g_value_get_uint64 (&value), NULL);
                break;
        case G_TYPE_CHAR:
                gconf_client_set_int (bridge->client, binding->key,
                                      g_value_get_char (&value), NULL);
                break;
        case G_TYPE_UCHAR:
                gconf_client_set_int (bridge->client, binding->key,
                                      g_value_get_uchar (&value), NULL);
                break;
        case G_TYPE_ENUM:
                gconf_client_set_int (bridge->client, binding->key,
                                      g_value_get_enum (&value), NULL);
                break;
        case G_TYPE_BOOLEAN:
                gconf_client_set_bool (bridge->client, binding->key,
                                       g_value_get_boolean (&value), NULL);
                break;
        case G_TYPE_DOUBLE:
                gconf_client_set_float (bridge->client, binding->key,
                                        g_value_get_double (&value), NULL);
                break;
        case G_TYPE_FLOAT:
                gconf_client_set_float (bridge->client, binding->key,
                                        g_value_get_float (&value), NULL);
                break;
        default:
                if (g_type_is_a (value.g_type, G_TYPE_ENUM)) {
                        gconf_client_set_int (bridge->client, binding->key,
                                              g_value_get_enum (&value), NULL);
                } else {
                        g_warning ("prop_binding_sync_prop_to_pref: "
                                   "Unhandled value type '%s'.\n",
                                   g_type_name (value.g_type));
                }

                break;
        }

        g_value_unset (&value);
}

/* Called when a GConf value bound to an object property has changed */
static void
prop_binding_pref_changed (GConfClient *client,
                           guint        cnxn_id,
                           GConfEntry  *entry,
                           gpointer     user_data)
{
        PropBinding *binding;
        GConfValue *val;

        binding = (PropBinding *) user_data;

        /* Make sure we don't enter an infinite synchronizing loop */
        if (binding->blocked)
                return;
        binding->blocked = TRUE;

        val = gconf_entry_get_value (entry);
        /* NULL means that the value has been unset */
        if (val)
                prop_binding_sync_pref_to_prop (binding, val);

        binding->blocked = FALSE;
}

/* Performs a scheduled prop-to-pref sync for a prop binding in 
 * delay mode */
static gboolean
prop_binding_perform_scheduled_sync (PropBinding *binding)
{
        binding->blocked = TRUE;

        prop_binding_sync_prop_to_pref (binding);

        binding->sync_timeout_id = 0;

        g_object_unref (binding->object);
        
        binding->blocked = FALSE;

        return FALSE;
}

#define PROP_BINDING_SYNC_DELAY 100 /* Delay for bindings with "delayed"
                                       set to TRUE, in ms */

/* Called when an object property has changed */
static void
prop_binding_prop_changed (GObject     *object,
                           GParamSpec  *param_spec,
                           PropBinding *binding)
{
        /* Make sure we don't enter an infinite synchronizing loop */
        if (binding->blocked)
                return;
        binding->blocked = TRUE;

        if (binding->delayed_mode) {
                /* Just schedule a sync */
                if (binding->sync_timeout_id == 0) {
                        /* We keep a reference on the object as long as
                         * we haven't synced yet to make sure we don't
                         * lose any data */
                        g_object_ref (binding->object);

                        binding->sync_timeout_id =
                                g_timeout_add
                                        (PROP_BINDING_SYNC_DELAY,
                                         (GSourceFunc)
                                            prop_binding_perform_scheduled_sync,
                                         binding);
                }
        } else {
                /* Directly sync */
                prop_binding_sync_prop_to_pref (binding);
        }

        binding->blocked = FALSE;
}

/* Called when an object is destroyed */
static void
prop_binding_object_destroyed (gpointer user_data,
                               GObject *where_the_object_was)
{
        PropBinding *binding;

        binding = (PropBinding *) user_data;
        binding->object = NULL; /* Don't do anything with the object
                                   at unbind() */
        
        g_hash_table_remove (bridge->bindings,
                             GUINT_TO_POINTER (binding->id));
}

/**
 * gconf_bridge_bind_property_full
 * @bridge: A #GConfBridge
 * @key: A GConf key to be bound
 * @object: A #GObject
 * @prop: The property of @object to be bound
 * @delayed_sync: TRUE if there should be a delay between property changes
 * and syncs to GConf. Set to TRUE when binding to a rapidly-changing
 * property, for example the "value" property on a #GtkAdjustment.
 *
 * Binds @key to @prop, causing them to have the same value at all times.
 *
 * The types of @key and @prop should be compatible. Floats and doubles, and
 * ints, uints, longs, unlongs, int64s, uint64s, chars, uchars and enums
 * can be matched up. Booleans and strings can only be matched to their
 * respective types.
 *
 * On calling this function the current value of @key will be set to @prop.
 *
 * Return value: The ID of the new binding.
 **/
guint
gconf_bridge_bind_property_full (GConfBridge *bridge,
                                 const char  *key,
                                 GObject     *object,
                                 const char  *prop,
                                 gboolean     delayed_sync)
{
        GParamSpec *pspec;
        PropBinding *binding;
        char *signal;
        GConfValue *val;

        g_return_val_if_fail (bridge != NULL, 0);
        g_return_val_if_fail (key != NULL, 0);
        g_return_val_if_fail (G_IS_OBJECT (object), 0);
        g_return_val_if_fail (prop != NULL, 0);

        /* First, try to fetch the propertys GParamSpec off the object */
        pspec = g_object_class_find_property
                                (G_OBJECT_GET_CLASS (object), prop);
        if (G_UNLIKELY (pspec == NULL)) {
                g_warning ("gconf_bridge_bind_property_full: A property \"%s\" "
                           "was not found. Please make sure you are passing "
                           "the right property name.", prop);

                return 0;
        }

        /* GParamSpec found: All good, create new binding. */
        binding = g_new (PropBinding, 1);

        binding->type = BINDING_PROP;
        binding->id = new_id ();
        binding->delayed_mode = delayed_sync;
        binding->key = g_strdup (key);
        binding->object = object;
        binding->prop = pspec;
        binding->sync_timeout_id = 0;
        binding->blocked = FALSE;
        
        /* Watch GConf key */
        binding->val_notify_id =
                gconf_client_notify_add (bridge->client, key,
                                         prop_binding_pref_changed,
                                         binding, NULL, NULL);

        /* Sync object to value from GConf, if set */
        val = gconf_client_get (bridge->client, key, NULL);
        if (val) {
                prop_binding_sync_pref_to_prop (binding, val);
                gconf_value_free (val);
        }

        /* Connect to property change notifications */
        signal = g_strconcat ("notify::", prop, NULL);
        binding->prop_notify_id =
                g_signal_connect (object, signal,
                                  G_CALLBACK (prop_binding_prop_changed),
                                  binding);
        g_free (signal);

        /* Handle case where watched object gets destroyed */
        g_object_weak_ref (object,
                           prop_binding_object_destroyed, binding);

        /* Insert binding */
        g_hash_table_insert (bridge->bindings,
                             GUINT_TO_POINTER (binding->id), binding);

        /* Done */
        return binding->id;
}

/* Unbinds a property binding */
static void
prop_binding_unbind (PropBinding *binding)
{
        if (binding->delayed_mode && binding->sync_timeout_id > 0) {
                /* Perform any scheduled syncs */
                g_source_remove (binding->sync_timeout_id);
                        
                /* The object will still be around as we have
                 * a reference */
                prop_binding_perform_scheduled_sync (binding);
        }

        gconf_client_notify_remove (bridge->client,
                                    binding->val_notify_id);
        g_free (binding->key);

        /* The object might have been destroyed .. */
        if (binding->object) {
                g_signal_handler_disconnect (binding->object,
                                             binding->prop_notify_id);

                g_object_weak_unref (binding->object,
                                     prop_binding_object_destroyed, binding);
        }
}

/*
 * Window bindings
 */

/* Performs a scheduled dimensions-to-prefs sync for a window binding */
static gboolean
window_binding_perform_scheduled_sync (WindowBinding *binding)
{
        if (binding->width_key) {
                int width, height;

                gtk_window_get_size (binding->window, &width, &height);

                gconf_client_set_int (bridge->client,
                                      binding->width_key, width,
                                      NULL);

                gconf_client_set_int (bridge->client,
                                      binding->height_key, height,
                                      NULL);
        }

        if (binding->x_key) {
                int x, y;

                gtk_window_get_position (binding->window, &x, &y);

                gconf_client_set_int (bridge->client,
                                      binding->x_key, x,
                                      NULL);

                gconf_client_set_int (bridge->client,
                                      binding->y_key, y,
                                      NULL);
        }

        binding->sync_timeout_id = 0;

        return FALSE;
}

#define WINDOW_BINDING_SYNC_DELAY 1000 /* Delay before syncing new window
                                          dimensions to GConf, in ms */

/* Called when the window han been resized or moved */
static gboolean
window_binding_configure_event_cb (GtkWindow         *window,
                                   GdkEventConfigure *event,
                                   WindowBinding     *binding)
{
        /* Schedule a sync */
        if (binding->sync_timeout_id == 0) {
                binding->sync_timeout_id =
                        g_timeout_add (WINDOW_BINDING_SYNC_DELAY,
                                       (GSourceFunc)
                                          window_binding_perform_scheduled_sync,
                                       binding);
        }

        return FALSE;
}

/* Called when the window is being unmapped */
static gboolean
window_binding_unmap_cb (GtkWindow     *window,
                         WindowBinding *binding)
{
        /* Force sync */
        if (binding->sync_timeout_id > 0)
                g_source_remove (binding->sync_timeout_id);

        window_binding_perform_scheduled_sync (binding);

        return FALSE;
}

/* Called when a window is destroyed */
static void
window_binding_window_destroyed (gpointer user_data,
                                 GObject *where_the_object_was)
{
        WindowBinding *binding;

        binding = (WindowBinding *) user_data;
        binding->window = NULL; /* Don't do anything with the window
                                   at unbind() */
        
        g_hash_table_remove (bridge->bindings,
                             GUINT_TO_POINTER (binding->id));
}

/**
 * gconf_bridge_bind_window
 * @bridge: A #GConfBridge
 * @key_prefix: The prefix of the GConf keys
 * @object: A #GtkWindow
 * @bind_size: TRUE to bind the size of @window
 * @bind_pos: TRUE to bind the position of @window
 * 
 * On calling this function @window will be resized to the values
 * specified by "@key_prefix _width" and "@key_prefix _height" if @bind_size
 * is TRUE, and moved to the values specified by "@key_prefix _x" and
 * "@key_prefix _y" if @bind_pos is TRUE.
 * The respective GConf values will be updated when the window is resized
 * and/or moved.
 *
 * Return value: The ID of the new binding.
 **/
guint
gconf_bridge_bind_window (GConfBridge *bridge,
                          const char  *key_prefix,
                          GtkWindow   *window,
                          gboolean     bind_size,
                          gboolean     bind_pos)
{
        WindowBinding *binding;

        g_return_val_if_fail (bridge != NULL, 0);
        g_return_val_if_fail (key_prefix != NULL, 0);
        g_return_val_if_fail (GTK_IS_WINDOW (window), 0);

        /* Create new binding. */
        binding = g_new (WindowBinding, 1);

        binding->type = BINDING_WINDOW;
        binding->id = new_id ();
        binding->window = window;
        binding->sync_timeout_id = 0;

        /* Set up GConf keys & sync window to GConf values */
        if (bind_size) {
                GConfValue *width_val, *height_val;

                binding->width_key = g_strconcat (key_prefix, "_width", NULL);
                binding->height_key = g_strconcat (key_prefix, "_height", NULL);
                
                width_val = gconf_client_get (bridge->client,
                                              binding->width_key, NULL);
                height_val = gconf_client_get (bridge->client,
                                               binding->height_key, NULL);
                if (width_val && height_val) {
                        gtk_window_resize (window,
                                           gconf_value_get_int (width_val),
                                           gconf_value_get_int (height_val));

                        gconf_value_free (width_val);
                        gconf_value_free (height_val);
                } else if (width_val) {
                        gconf_value_free (width_val);
                } else if (height_val) {
                        gconf_value_free (height_val);
                }
        } else {
                binding->width_key = NULL;
                binding->height_key = NULL;
        }

        if (bind_pos) {
                GConfValue *x_val, *y_val;
                
                binding->x_key = g_strconcat (key_prefix, "_x", NULL);
                binding->y_key = g_strconcat (key_prefix, "_y", NULL);

                x_val = gconf_client_get (bridge->client,
                                          binding->x_key, NULL);
                y_val = gconf_client_get (bridge->client,
                                          binding->y_key, NULL);
                if (x_val && y_val) {
                        gtk_window_move (window,
                                         gconf_value_get_int (x_val),
                                         gconf_value_get_int (y_val));

                        gconf_value_free (x_val);
                        gconf_value_free (y_val);
                } else if (x_val) {
                        gconf_value_free (x_val);
                } else if (y_val) {
                        gconf_value_free (y_val);
                }
        } else {
                binding->x_key = NULL;
                binding->y_key = NULL;
        }

        /* Connect to window size change notifications */
        binding->configure_event_id =
                g_signal_connect (window,
                                  "configure-event",
                                  G_CALLBACK
                                        (window_binding_configure_event_cb),
                                  binding);
        binding->unmap_id =
                g_signal_connect (window,
                                  "unmap",
                                  G_CALLBACK (window_binding_unmap_cb),
                                  binding);

        /* Handle case where window gets destroyed */
        g_object_weak_ref (G_OBJECT (window),
                           window_binding_window_destroyed, binding);

        /* Insert binding */
        g_hash_table_insert (bridge->bindings,
                             GUINT_TO_POINTER (binding->id), binding);

        /* Done */
        return binding->id;
}

/* Unbinds a window binding */
static void
window_binding_unbind (WindowBinding *binding)
{
        if (binding->sync_timeout_id > 0)
                g_source_remove (binding->sync_timeout_id);

        g_free (binding->width_key);
        g_free (binding->height_key);
        g_free (binding->x_key);
        g_free (binding->y_key);

        /* The window might have been destroyed .. */
        if (binding->window) {
                g_signal_handler_disconnect (binding->window,
                                             binding->configure_event_id);
                g_signal_handler_disconnect (binding->window,
                                             binding->unmap_id);

                g_object_weak_unref (G_OBJECT (binding->window),
                                     window_binding_window_destroyed, binding);
        }
}

/*
 * List store bindings
 */

/* Fills a GtkListStore with the string list from @value */
static void
list_store_binding_sync_pref_to_store (ListStoreBinding *binding,
                                       GConfValue       *value)
{
        GSList *list, *l;
        GtkTreeIter iter;
        
        gtk_list_store_clear (binding->list_store);

        list = gconf_value_get_list (value);
        for (l = list; l; l = l->next) {
                GConfValue *l_value;
                const char *string;

                l_value = (GConfValue *) l->data;
                string = gconf_value_get_string (l_value);

                gtk_list_store_insert_with_values (binding->list_store,
                                                   &iter, -1,
                                                   0, string,
                                                   -1);
        }
}

/* Sets a GConf value to the contents of a GtkListStore */
static gboolean
list_store_binding_sync_to_pref (ListStoreBinding *binding)
{
        GtkTreeModel *tree_model;
        GtkTreeIter iter;
        GSList *list;
        int res;

        tree_model = GTK_TREE_MODEL (binding->list_store);

        /* Build list */
        list = NULL;
        res = gtk_tree_model_get_iter_first (tree_model, &iter);
        while (res) {
                char *string;

                gtk_tree_model_get (tree_model, &iter,
                                    0, &string, -1);
                list = g_slist_append (list, string);

                res = gtk_tree_model_iter_next (tree_model, &iter);
        }

        /* Set */
        gconf_client_set_list (bridge->client, binding->key,
                               GCONF_VALUE_STRING, list, NULL);

        /* Cleanup */
        while (list) {
                char *string;

                string = list->data;
                list = g_slist_delete_link (list, list);
                g_free (string);
        }

        binding->sync_idle_id = 0;

        g_object_unref (binding->list_store);

        return FALSE;
}

/* Pref changed: sync */
static void
list_store_binding_pref_changed (GConfClient *client,
                                 guint        cnxn_id,
                                 GConfEntry  *entry,
                                 gpointer     user_data)
{
        ListStoreBinding *binding;
        GConfValue *val;

        binding = (ListStoreBinding *) user_data;

        /* Make sure we don't enter an infinite synchronizing loop */
        if (binding->blocked)
                return;
        binding->blocked = TRUE;

        val = gconf_entry_get_value (entry);
        /* NULL means that the value has been unset */
        if (val)
                list_store_binding_sync_pref_to_store (binding, val);

        binding->blocked = FALSE;
}

/* Called when an object is destroyed */
static void
list_store_binding_store_destroyed (gpointer user_data,
                                    GObject *where_the_object_was)
{
        ListStoreBinding *binding;

        binding = (ListStoreBinding *) user_data;
        binding->list_store = NULL; /* Don't do anything with the store
                                       at unbind() */
        
        g_hash_table_remove (bridge->bindings,
                             GUINT_TO_POINTER (binding->id));
}

/* List store changed: Sync */
static void
list_store_binding_store_changed_cb (ListStoreBinding *binding)
{
        if (binding->sync_idle_id == 0) {
                g_object_ref (binding->list_store);

                binding->sync_idle_id = g_idle_add
                        ((GSourceFunc) list_store_binding_sync_to_pref,
                         binding);
        }
}

/**
 * gconf_bridge_bind_string_list_store
 * @bridge: A #GConfBridge
 * @key: A GConf key to be bound
 * @list_store: A #GtkListStore
 * 
 * On calling this function single string column #GtkListStore @list_store
 * will be kept synchronized with the GConf string list value pointed to by
 * @key. On calling this function @list_store will be populated with the
 * strings specified by the value of @key.
 *
 * Return value: The ID of the new binding.
 **/
guint
gconf_bridge_bind_string_list_store (GConfBridge  *bridge,
                                     const char   *key,
                                     GtkListStore *list_store)
{
        GtkTreeModel *tree_model;
        gboolean have_one_column, is_string_column;
        ListStoreBinding *binding;
        GConfValue *val;

        g_return_val_if_fail (bridge != NULL, 0);
        g_return_val_if_fail (key != NULL, 0);
        g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), 0);

        /* Check list store suitability */
        tree_model = GTK_TREE_MODEL (list_store);
        have_one_column = (gtk_tree_model_get_n_columns (tree_model) == 1);
        if (have_one_column) {
                is_string_column = (gtk_tree_model_get_column_type
                                        (tree_model, 0) == G_TYPE_STRING);
        }
        if (G_UNLIKELY (!have_one_column || !is_string_column)) {
                g_warning ("gconf_bridge_bind_string_list_store: Only "
                           "GtkListStores with exactly one string column are "
                           "supported.");

                return 0;
        }

        /* Create new binding. */
        binding = g_new (ListStoreBinding, 1);

        binding->type = BINDING_LIST_STORE;
        binding->id = new_id ();
        binding->key = g_strdup (key);
        binding->list_store = list_store;
        binding->sync_idle_id = 0;
        binding->blocked = FALSE;

        /* Watch GConf key */
        binding->val_notify_id =
                gconf_client_notify_add (bridge->client, key,
                                         list_store_binding_pref_changed,
                                         binding, NULL, NULL);

        /* Sync object to value from GConf, if set */
        val = gconf_client_get (bridge->client, key, NULL);
        if (val) {
                list_store_binding_sync_pref_to_store (binding, val);
                gconf_value_free (val);
        }

        /* Connect to ListStore change notifications */
        binding->row_inserted_id =
                g_signal_connect_swapped (list_store, "row-inserted",
                                          G_CALLBACK
                                          (list_store_binding_store_changed_cb),
                                          binding);
        binding->row_changed_id =
                g_signal_connect_swapped (list_store, "row-inserted",
                                          G_CALLBACK
                                          (list_store_binding_store_changed_cb),
                                          binding);
        binding->row_deleted_id =
                g_signal_connect_swapped (list_store, "row-inserted",
                                          G_CALLBACK
                                          (list_store_binding_store_changed_cb),
                                          binding);
        binding->rows_reordered_id =
                g_signal_connect_swapped (list_store, "row-inserted",
                                          G_CALLBACK
                                          (list_store_binding_store_changed_cb),
                                          binding);

        /* Handle case where watched object gets destroyed */
        g_object_weak_ref (G_OBJECT (list_store),
                           list_store_binding_store_destroyed, binding);

        /* Insert binding */
        g_hash_table_insert (bridge->bindings,
                             GUINT_TO_POINTER (binding->id), binding);

        /* Done */
        return binding->id;
}

/* Unbinds a list store binding */
static void
list_store_binding_unbind (ListStoreBinding *binding)
{
        /* Perform any scheduled syncs */
        if (binding->sync_idle_id > 0) {
                g_source_remove (binding->sync_idle_id);

                /* The store will still be around as we added a reference */
                list_store_binding_sync_to_pref (binding);
        }

        g_free (binding->key);

        /* The store might have been destroyed .. */
        if (binding->list_store) {
                g_signal_handler_disconnect (binding->list_store,
                                             binding->row_inserted_id);
                g_signal_handler_disconnect (binding->list_store,
                                             binding->row_changed_id);
                g_signal_handler_disconnect (binding->list_store,
                                             binding->row_deleted_id);
                g_signal_handler_disconnect (binding->list_store,
                                             binding->rows_reordered_id);

                g_object_weak_unref (G_OBJECT (binding->list_store),
                                     list_store_binding_store_destroyed,
                                     binding);
        }
}

/*
 * Generic unbinding
 */

/* Unbinds a binding */
static void
unbind (Binding *binding)
{
        /* Call specialized unbinding function */
        switch (binding->type) {
        case BINDING_PROP:
                prop_binding_unbind ((PropBinding *) binding);
                break;
        case BINDING_WINDOW:
                window_binding_unbind ((WindowBinding *) binding);
                break;
        case BINDING_LIST_STORE:
                list_store_binding_unbind ((ListStoreBinding *) binding);
                break;
        default:
                g_warning ("Unknown binding type '%d'\n", binding->type);
                break;
        }

        g_free (binding);
}

/**
 * gconf_bridge_unbind
 * @bridge: A #GConfBridge
 * @binding_id: The ID of the binding to be removed
 *
 * Removes the binding with ID @binding_id.
 **/
void
gconf_bridge_unbind (GConfBridge *bridge,
                     guint        binding_id)
{
        g_return_if_fail (bridge != NULL);
        g_return_if_fail (binding_id > 0);

        /* This will trigger the hash tables value destruction
         * function, which will take care of further cleanup */
        g_hash_table_remove (bridge->bindings, 
                             GUINT_TO_POINTER (binding_id));
}

/*
 * Error handling
 */

/* This is the same dialog as used in eel */
static void
error_handler (GConfClient *client,
               GError      *error)
{
        static gboolean shown_dialog = FALSE;

        g_warning ("GConf error:\n  %s", error->message);

        if (!shown_dialog) {
                char *message;
                GtkWidget *dlg;

                message = g_strdup_printf (_("GConf error: %s"),
                                           error->message);
                dlg = gtk_message_dialog_new (NULL, 0,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_OK,
                                              message);
                g_free (message);

                gtk_message_dialog_format_secondary_text
                        (GTK_MESSAGE_DIALOG (dlg),
                         _("All further errors shown only on terminal."));
                gtk_window_set_title (GTK_WINDOW (dlg), "");

                gtk_dialog_run (GTK_DIALOG (dlg));

                gtk_widget_destroy (dlg);

                shown_dialog = TRUE;
	}
}

/**
 * gconf_bridge_install_default_error_handler
 *
 * Sets up the default error handler. Any unhandled GConf errors will
 * automatically be handled by presenting the user an error dialog.
 **/
void
gconf_bridge_install_default_error_handler (void)
{
        gconf_client_set_global_default_error_handler (error_handler);
}
