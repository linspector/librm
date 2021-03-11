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

#ifndef __RM_NOTIFICATION_H__
#define __RM_NOTIFICATION_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <glib.h>

#include <rm/rmconnection.h>
#include <rm/rmcontact.h>

G_BEGIN_DECLS


typedef struct RmNotificationMessage RmNotificationMessage;
typedef struct RmNotification RmNotification;

/**
 * RmNotificationMessage:
 *
 * The #RmNotificationMessage-struct contains only private fileds and should not be directly accessed.
 */
struct RmNotificationMessage {
	/*< private >*/
	gpointer priv;
	RmConnection *connection;
	RmNotification *notification;
};

/**
 * RmNotification:
 *
 * The #RmNotification-struct contains only private fileds and should not be directly accessed.
 */
struct RmNotification {
	/*< private >*/
	gchar *name;
	gpointer (*show)(RmConnection *connection, RmContact *contact);
	void (*update)(RmConnection *connection, RmContact *contact);
	void (*close)(gpointer priv);
};

RmNotification *rm_notification_get(gchar *name);

void rm_notification_init(void);
void rm_notification_shutdown(void);

void rm_notification_register(RmNotification *notification);
void rm_notification_unregister(RmNotification *notification);

GList *rm_notification_get_plugins(void);
gchar *rm_notification_get_name(RmNotification *notification);
void rm_notification_message_close(RmNotificationMessage *message);
RmNotificationMessage *rm_notification_message_get(RmConnection *connection);

G_END_DECLS

#endif
