/*
 * The rm project
 * Copyright (c) 2012-2017 Jan-Michael Brummer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <string.h>

#include <gio/gio.h>

#include <rm/rm.h>

/**
 * gnotification_close:
 * @priv: private data (notification)
 *
 * Close gnotification window
 */
void gnotification_close(gpointer priv)
{
	g_application_withdraw_notification(G_APPLICATION(g_application_get_default()), priv);
}

/**
 * gnotification_timeout_close:
 * @priv: private data (notification)
 *
 * Close notification after time out
 *
 * Returns: %G_SOURCE_REMOVE
 */
gboolean gnotification_timeout_close(gpointer priv)
{
	gnotification_close(priv);

	return G_SOURCE_REMOVE;
}

/**
 * gnotification_show_missed_calls:
 *
 * Create and show notification about missed calls
 */
void gnotification_show_missed_calls(void)
{
	GNotification *notify = NULL;
	gchar *text = NULL;

	g_application_withdraw_notification(G_APPLICATION(g_application_get_default()), "missed-calls");

	/* Create notification message */
	text = g_strdup_printf(R_("You have missed calls"));

	notify = g_notification_new(R_("Missed call(s)"));

	g_notification_set_body(notify, text);
	g_free(text);

	g_notification_add_button_with_target(notify, R_("Open journal"), "app.journal", NULL);
	g_application_send_notification(G_APPLICATION(g_application_get_default()), "missed-calls", notify);
	g_object_unref(notify);
}

/**
 * gnotification_show:
 * @connection: a #RmConnection
 * @contact: a #RmContact
 *
 * Create and show connection notification
 *
 * Returns: a unique id for the new notifiation or %NULL on error
 */
gpointer gnotification_show(RmConnection *connection, RmContact *contact)
{
	GNotification *notify = NULL;
	GIcon *icon;
	gchar *title;
	gchar *text;
	gchar *uid;

	if (connection->type != RM_CONNECTION_TYPE_INCOMING && connection->type != RM_CONNECTION_TYPE_OUTGOING) {
		g_warning("Unhandled case in connection notify - gnotification!");

		return NULL;
	}
	/* Create notification message */
	text = g_markup_printf_escaped(R_("Name:\t\t%s\nNumber:\t\t%s\nCompany:\t%s\nStreet:\t\t%s\nCity:\t\t\t%s%s%s\n"),
				       contact->name ? contact->name : "",
				       contact->number ? contact->number : "",
				       contact->company ? contact->company : "",
				       contact->street ? contact->street : "",
				       contact->zip ? contact->zip : "",
				       contact->zip ? " " : "",
				       contact->city ? contact->city : ""
				       );

	if (connection->type == RM_CONNECTION_TYPE_INCOMING) {
		title = g_strdup_printf(R_("Incoming call (on %s)"), connection->local_number);
	} else {
		title = g_strdup_printf(R_("Outgoing call (on %s)"), connection->local_number);
	}

	notify = g_notification_new(title);
	g_free(title);

	g_notification_set_body(notify, text);
	g_free(text);

	uid = g_strdup_printf("%s%s", connection->local_number, contact->number);

	if (connection->type == (RM_CONNECTION_TYPE_INCOMING | RM_CONNECTION_TYPE_SOFTPHONE)) {
		g_notification_add_button_with_target(notify, R_("Accept"), "app.pickup", "i", connection->id);
		g_notification_add_button_with_target(notify, R_("Decline"), "app.hangup", "i", connection->id);
	} else if (connection->type == RM_CONNECTION_TYPE_OUTGOING) {
		gint duration = 5;

		g_timeout_add_seconds(duration, gnotification_timeout_close, uid);
	}

	if (contact->image) {
		icon = G_ICON(contact->image);
		g_notification_set_icon(notify, icon);
	}

	g_notification_set_priority(notify, G_NOTIFICATION_PRIORITY_URGENT);
	g_application_send_notification(G_APPLICATION(g_application_get_default()), uid, notify);
	g_object_unref(notify);

	return uid;
}

/**
 * gnotification_update:
 * @connection: a #RmConnection
 * @contact: a #RmContact
 *
 * Update notification with new information
 */
void gnotification_update(RmConnection *connection, RmContact *contact)
{
}

RmNotification gnotification = {
	"GNotification",
	gnotification_show,
	gnotification_update,
	gnotification_close,
};

/**
 * gnotification_plugin_init:
 * @plugin: a #RmPlugin
 *
 * Activate plugin
 *
 * Returns: %TRUE
 */
gboolean gnotification_plugin_init(RmPlugin *plugin)
{
	rm_notification_register(&gnotification);

	return TRUE;
}

/**
 * gnotification_plugin_shutdown:
 * @plugin: a #RmPlugin
 *
 * Deactivate plugin
 *
 * Returns: %TRUE
 */
gboolean gnotification_plugin_shutdown(RmPlugin *plugin)
{
	GApplication *app = g_application_get_default();

	if (app) {
		g_application_withdraw_notification(G_APPLICATION(app), "missed-calls");
	}

	rm_notification_unregister(&gnotification);

	return TRUE;
}

RM_PLUGIN(gnotification);
