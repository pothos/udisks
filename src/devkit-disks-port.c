/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 David Zeuthen <david@fubar.dk>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gio/gunixmounts.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gudev/gudev.h>
#include <stdlib.h>

#include "devkit-disks-daemon.h"
#include "devkit-disks-port.h"
#include "devkit-disks-port-private.h"
#include "devkit-disks-marshal.h"

#include "devkit-disks-adapter.h"

/*--------------------------------------------------------------------------------------------------------------*/
#include "devkit-disks-port-glue.h"

static void     devkit_disks_port_class_init  (DevkitDisksPortClass *klass);
static void     devkit_disks_port_init        (DevkitDisksPort      *seat);
static void     devkit_disks_port_finalize    (GObject     *object);

static gboolean update_info                (DevkitDisksPort *port);

static void     drain_pending_changes (DevkitDisksPort *port, gboolean force_update);

enum
{
        PROP_0,
        PROP_NATIVE_PATH,

        PROP_ADAPTER,
        PROP_PARENT,
        PROP_NUMBER,
};

enum
{
        CHANGED_SIGNAL,
        LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DevkitDisksPort, devkit_disks_port, G_TYPE_OBJECT)

#define DEVKIT_DISKS_PORT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DEVKIT_DISKS_TYPE_PORT, DevkitDisksPortPrivate))

static void
get_property (GObject         *object,
              guint            prop_id,
              GValue          *value,
              GParamSpec      *pspec)
{
        DevkitDisksPort *port = DEVKIT_DISKS_PORT (object);

        switch (prop_id) {
        case PROP_NATIVE_PATH:
                g_value_set_string (value, port->priv->native_path);
                break;

        case PROP_ADAPTER:
                if (port->priv->adapter != NULL)
                        g_value_set_boxed (value, port->priv->adapter);
                else
                        g_value_set_boxed (value, "/");
                break;

        case PROP_PARENT:
                if (port->priv->parent != NULL)
                        g_value_set_boxed (value, port->priv->parent);
                else
                        g_value_set_boxed (value, "/");
                break;

        case PROP_NUMBER:
                g_value_set_int (value, port->priv->number);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
devkit_disks_port_class_init (DevkitDisksPortClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = devkit_disks_port_finalize;
        object_class->get_property = get_property;

        g_type_class_add_private (klass, sizeof (DevkitDisksPortPrivate));

        signals[CHANGED_SIGNAL] =
                g_signal_new ("changed",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        dbus_g_object_type_install_info (DEVKIT_DISKS_TYPE_PORT, &dbus_glib_devkit_disks_port_object_info);

        g_object_class_install_property (
                object_class,
                PROP_NATIVE_PATH,
                g_param_spec_string ("native-path", NULL, NULL, NULL, G_PARAM_READABLE));
        g_object_class_install_property (
                object_class,
                PROP_ADAPTER,
                g_param_spec_boxed ("adapter", NULL, NULL, DBUS_TYPE_G_OBJECT_PATH, G_PARAM_READABLE));
        g_object_class_install_property (
                object_class,
                PROP_PARENT,
                g_param_spec_boxed ("parent", NULL, NULL, DBUS_TYPE_G_OBJECT_PATH, G_PARAM_READABLE));
        g_object_class_install_property (
                object_class,
                PROP_NUMBER,
                g_param_spec_int ("number", NULL, NULL, G_MININT, G_MAXINT, -1, G_PARAM_READABLE));
}

static void
devkit_disks_port_init (DevkitDisksPort *port)
{
        port->priv = DEVKIT_DISKS_PORT_GET_PRIVATE (port);
        port->priv->number = -1;
}

static void
devkit_disks_port_finalize (GObject *object)
{
        DevkitDisksPort *port;

        g_return_if_fail (object != NULL);
        g_return_if_fail (DEVKIT_DISKS_IS_PORT (object));

        port = DEVKIT_DISKS_PORT (object);
        g_return_if_fail (port->priv != NULL);

        /* g_debug ("finalizing %s", port->priv->native_path); */

        g_object_unref (port->priv->d);
        g_object_unref (port->priv->daemon);
        g_free (port->priv->object_path);

        g_free (port->priv->native_path);
        g_free (port->priv->native_path_for_device_prefix);

        if (port->priv->emit_changed_idle_id > 0)
                g_source_remove (port->priv->emit_changed_idle_id);

        /* free properties */
        g_free (port->priv->adapter);
        g_free (port->priv->parent);

        G_OBJECT_CLASS (devkit_disks_port_parent_class)->finalize (object);
}

/**
 * compute_object_path:
 * @port: A #DevkitDisksPort.
 *
 * Computes the D-Bus object path for the port.
 *
 * Returns: A valid D-Bus object path. Free with g_free().
 */
static char *
compute_object_path (DevkitDisksPort *port)
{
        const gchar *basename;
        GString *s;
        guint n;

        basename = strrchr (port->priv->native_path, '/');
        if (basename != NULL) {
                basename++;
        } else {
                basename = port->priv->native_path;
        }

        s = g_string_new (port->priv->parent);
        g_string_append_c (s, '/');
        for (n = 0; basename[n] != '\0'; n++) {
                gint c = basename[n];

                /* D-Bus spec sez:
                 *
                 * Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_"
                 */
                if ((c >= 'A' && c <= 'Z') ||
                    (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9')) {
                        g_string_append_c (s, c);
                } else {
                        /* Escape bytes not in [A-Z][a-z][0-9] as _<hex-with-two-digits> */
                        g_string_append_printf (s, "_%02x", c);
                }
        }

        return g_string_free (s, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
register_disks_port (DevkitDisksPort *port)
{
        DBusConnection *connection;
        GError *error = NULL;

        port->priv->system_bus_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (port->priv->system_bus_connection == NULL) {
                if (error != NULL) {
                        g_critical ("error getting system bus: %s", error->message);
                        g_error_free (error);
                }
                goto error;
        }
        connection = dbus_g_connection_get_connection (port->priv->system_bus_connection);

        port->priv->object_path = compute_object_path (port);

        /* safety first */
        if (dbus_g_connection_lookup_g_object (port->priv->system_bus_connection,
                                               port->priv->object_path) != NULL) {
                g_error ("**** HACK: Wanting to register object at path `%s' but there is already an "
                         "object there. This is an internal error in the daemon. Aborting.\n",
                         port->priv->object_path);
        }

        dbus_g_connection_register_g_object (port->priv->system_bus_connection,
                                             port->priv->object_path,
                                             G_OBJECT (port));

        return TRUE;

error:
        return FALSE;
}

void
devkit_disks_port_removed (DevkitDisksPort *port)
{
        port->priv->removed = TRUE;

        dbus_g_connection_unregister_g_object (port->priv->system_bus_connection,
                                               G_OBJECT (port));
        g_assert (dbus_g_connection_lookup_g_object (port->priv->system_bus_connection,
                                                     port->priv->object_path) == NULL);
}

DevkitDisksPort *
devkit_disks_port_new (DevkitDisksDaemon *daemon, GUdevDevice *d)
{
        DevkitDisksPort *port;
        const char *native_path;

        port = NULL;
        native_path = g_udev_device_get_sysfs_path (d);

        port = DEVKIT_DISKS_PORT (g_object_new (DEVKIT_DISKS_TYPE_PORT, NULL));
        port->priv->d = g_object_ref (d);
        port->priv->daemon = g_object_ref (daemon);
        port->priv->native_path = g_strdup (native_path);

        if (!update_info (port)) {
                g_object_unref (port);
                port = NULL;
                goto out;
        }

        if (!register_disks_port (DEVKIT_DISKS_PORT (port))) {
                g_object_unref (port);
                port = NULL;
                goto out;
        }

out:
        return port;
}

static void
drain_pending_changes (DevkitDisksPort *port, gboolean force_update)
{
        gboolean emit_changed;

        emit_changed = FALSE;

        /* the update-in-idle is set up if, and only if, there are pending changes - so
         * we should emit a 'change' event only if it is set up
         */
        if (port->priv->emit_changed_idle_id != 0) {
                g_source_remove (port->priv->emit_changed_idle_id);
                port->priv->emit_changed_idle_id = 0;
                emit_changed = TRUE;
        }

        if ((!port->priv->removed) && (emit_changed || force_update)) {
                if (port->priv->object_path != NULL) {
                        g_print ("**** EMITTING CHANGED for %s\n", port->priv->native_path);
                        g_signal_emit_by_name (port, "changed");
                        g_signal_emit_by_name (port->priv->daemon, "port-changed", port->priv->object_path);
                }
        }
}

/* called by the daemon on the 'change' uevent */
gboolean
devkit_disks_port_changed (DevkitDisksPort *port, GUdevDevice *d, gboolean synthesized)
{
        gboolean keep_port;

        g_object_unref (port->priv->d);
        port->priv->d = g_object_ref (d);

        keep_port = update_info (port);

        /* this 'change' event might prompt us to remove the port */
        if (!keep_port)
                goto out;

        /* no, it's good .. keep it.. and always force a 'change' signal if the event isn't synthesized */
        drain_pending_changes (port, !synthesized);

out:
        return keep_port;
}

/* ---------------------------------------------------------------------------------------------------- */

const char *
devkit_disks_port_local_get_object_path (DevkitDisksPort *port)
{
        return port->priv->object_path;
}

const char *
devkit_disks_port_local_get_native_path (DevkitDisksPort *port)
{
        return port->priv->native_path;
}

gboolean
devkit_disks_local_port_is_for_device (DevkitDisksPort *port,
                                       const gchar     *device_native_path)
{
        gboolean ret;

        ret = FALSE;

        if (port->priv->port_type == PORT_TYPE_ATA) {

                ret = g_str_has_prefix (device_native_path, port->priv->native_path_for_device_prefix);

        } else if (port->priv->port_type == PORT_TYPE_SAS) {
                GDir *dir;
                gchar *s;
                const gchar *name;
                const gchar *phy_kernel_name;

                phy_kernel_name = g_udev_device_get_name (port->priv->d);

                /* Typically it looks like this
                 *
                 *  .../host6/port-6:0/end_device-6:0/target6:0:0/6:0:0:0/block/sda
                 *
                 * where
                 *
                 *  # ls /sys/devices/pci0000:00/0000:00:01.0/0000:07:00.0/host6/port-6:0/
                 *  end_device-6:0  phy-6:0  power  sas_port  uevent
                 *
                 * unfortunately there are no helpful symlinks here so we need to
                 * use device/../../..
                 *
                 * TODO: Ugh, this is probably pretty expensive syscall-wise...
                 */
                s = g_strdup_printf ("%s/device/../../..", device_native_path);
                dir = g_dir_open (s, 0, NULL);
                if (dir != NULL) {
                        while ((name = g_dir_read_name (dir)) != NULL) {
                                if (g_strcmp0 (name, phy_kernel_name) == 0) {
                                        ret = TRUE;
                                        break;
                                }
                        }
                        g_dir_close (dir);
                }
                g_free (s);
        }

        return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static char *
sysfs_resolve_link (const char *sysfs_path, const char *name)
{
        char *full_path;
        char link_path[PATH_MAX];
        char resolved_path[PATH_MAX];
        ssize_t num;
        gboolean found_it;

        found_it = FALSE;

        full_path = g_build_filename (sysfs_path, name, NULL);

        //g_debug ("name='%s'", name);
        //g_debug ("full_path='%s'", full_path);
        num = readlink (full_path, link_path, sizeof (link_path) - 1);
        if (num != -1) {
                char *absolute_path;

                link_path[num] = '\0';

                //g_debug ("link_path='%s'", link_path);
                absolute_path = g_build_filename (sysfs_path, link_path, NULL);
                //g_debug ("absolute_path='%s'", absolute_path);
                if (realpath (absolute_path, resolved_path) != NULL) {
                        //g_debug ("resolved_path='%s'", resolved_path);
                        found_it = TRUE;
                }
                g_free (absolute_path);
        }
        g_free (full_path);

        if (found_it)
                return g_strdup (resolved_path);
        else
                return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

static gint
int_compare_func (gconstpointer a,
                  gconstpointer b)
{
        gint a_val;
        gint b_val;

        a_val = *((gint *) a);
        b_val = *((gint *) b);

        return a_val - b_val;
}

/* Update info for an ATA port */
static gboolean
update_info_ata (DevkitDisksPort    *port,
                 DevkitDisksAdapter *adapter)
{
        GDir *dir;
        GError *error;
        gboolean ret;
        const gchar *name;
        GArray *numbers;
        guint n;
        const gchar *basename;
        gint port_host_number;
        gint port_number;

        ret = FALSE;
        port_number = -1;
        dir = NULL;
        numbers = NULL;

        /* First, figure out prefix used for matching the device on the port */
        if (port->priv->native_path_for_device_prefix == NULL) {
                port->priv->native_path_for_device_prefix =
                        sysfs_resolve_link (g_udev_device_get_sysfs_path (port->priv->d),
                                            "device");
                if (port->priv->native_path_for_device_prefix == NULL) {
                        g_warning ("Unable to resolve 'device' symlink for %s",
                                   g_udev_device_get_sysfs_path (port->priv->d));
                        goto out;
                }
        }

        /* Second, figure out the port number.
         *
         * As ATA drivers create one scsi_host objects for each port
         * the port number can be inferred from the numbering of the
         * scsi_host objects.
         *
         * We also want to check that the scsi_host object *really*
         * stems from SATA. There doesn't seem to be an easy way of
         * doing this so the heuristic used right now is that it
         * the scsi_host is ATA if, and only if, there is more than
         * one scsi_host....
         *
         * (TODO: we could match on driver names instead? Anyway,
         *        there are plans to properly support the ATA
         *        topology (including PMP) once libata switches
         *        away from SCSI. Once this is available these
         *        hacks can go away...)
         */

        basename = strrchr (port->priv->native_path, '/');
        if (basename == NULL || sscanf (basename + 1, "host%d", &port_host_number) != 1) {
                g_warning ("Cannot extract port host number from %s", port->priv->native_path);
                goto out;
        }

        dir = g_dir_open (devkit_disks_adapter_local_get_native_path (adapter),
                          0,
                          &error);
        if (dir == NULL) {
                g_warning ("Unable to open %s: %s",
                           devkit_disks_adapter_local_get_native_path (adapter),
                           error->message);
                g_error_free (error);
                goto out;
        }

        numbers = g_array_new (FALSE, FALSE, sizeof (gint));

        while ((name = g_dir_read_name (dir)) != NULL) {
                gint number;

                if (sscanf (name, "host%d", &number) != 1)
                        continue;

                g_array_append_val (numbers, number);
        }

        if (numbers->len < 2) {
                /* This is (probably, see above) ATA since there isn't at least two
                 * scsi_host objects.
                 */
                goto out;
        }

        g_array_sort (numbers, int_compare_func);

        for (n = 0; n < numbers->len; n++) {
                gint number;

                number = ((gint *) numbers->data) [n];

                if (number == port_host_number) {
                        port_number = n;
                        break;
                }
        }

        devkit_disks_port_set_number (port, port_number);
        port->priv->port_type = PORT_TYPE_ATA;
        ret = TRUE;

 out:
        if (dir != NULL)
                g_dir_close (dir);
        if (numbers != NULL)
                g_array_unref (numbers);

        return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/* Update info for a SAS PHY */
static gboolean
update_info_sas_phy (DevkitDisksPort    *port,
                     DevkitDisksAdapter *adapter)
{
        gboolean ret;
        gint port_number;
        const gchar *kernel_name;
        guint level;
        guint n;

        ret = FALSE;
        port_number = -1;

        /* Count how deep this PHY is - a level of 1 indicates it's a PHY for the
         * HBA and not for an expander
         */
        level = 0;
        kernel_name = g_udev_device_get_name (port->priv->d);
        for (n = 0; kernel_name[n] != '\0'; n++) {
                if (kernel_name[n] == ':')
                        level++;
        }

        if (level > 1)
                goto out;

        port_number = g_udev_device_get_sysfs_attr_as_int (port->priv->d, "phy_identifier");

        devkit_disks_port_set_number (port, port_number);
        port->priv->port_type = PORT_TYPE_SAS;
        ret = TRUE;
 out:
        return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * update_info:
 * @port: the port
 *
 * Update information about the port.
 *
 * If one or more properties changed, the changes are scheduled to be emitted. Use
 * drain_pending_changes() to force emitting the pending changes (which is useful
 * before returning the result of an operation).
 *
 * Returns: #TRUE to keep (or add) the port; #FALSE to ignore (or remove) the port
 **/
static gboolean
update_info (DevkitDisksPort *port)
{
        gboolean ret;
        DevkitDisksAdapter *adapter;

        ret = FALSE;

        adapter = devkit_disks_daemon_local_find_adapter (port->priv->daemon,
                                                          port->priv->native_path);
        if (adapter == NULL)
                goto out;

        /* TODO: handle expansion devices here (SAS expanders, SATA Port Multipliers) */
        devkit_disks_port_set_adapter (port, devkit_disks_adapter_local_get_object_path (adapter));
        devkit_disks_port_set_parent (port, devkit_disks_adapter_local_get_object_path (adapter));

        if (g_strcmp0 (g_udev_device_get_subsystem (port->priv->d), "scsi_host") == 0) {
                if (!update_info_ata (port, adapter))
                        goto out;
        } else {
                if (!update_info_sas_phy (port, adapter))
                        goto out;
        }

        ret = TRUE;

 out:
        return ret;
}

