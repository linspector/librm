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

#ifndef __RM_CONNECTION_H__
#define __RM_CONNECTION_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <rm/rmdevice.h>

G_BEGIN_DECLS

/**
 * RmConnectionType:
 * @RM_CONNECTION_TYPE_INCOMING: Incoming connection
 * @RM_CONNECTION_TYPE_OUTGOING: Outgoing connection
 * @RM_CONNECTION_TYPE_CONNECT: Connection established
 * @RM_CONNECTION_TYPE_DISCONNECT: Connection terminated
 * @RM_CONNECTION_TYPE_MISSED: Connection missed (call not picked up)
 * @RM_CONNECTION_TYPE_SOFTPHONE: Additional flag indicating that this is a softphone connection
 *
 * The type of a connection.
 */
typedef enum {
	RM_CONNECTION_TYPE_INCOMING   = 0x01,
	RM_CONNECTION_TYPE_OUTGOING   = 0x02,
	RM_CONNECTION_TYPE_CONNECT    = 0x04,
	RM_CONNECTION_TYPE_DISCONNECT = 0x08,
	RM_CONNECTION_TYPE_MISSED     = (RM_CONNECTION_TYPE_INCOMING | RM_CONNECTION_TYPE_DISCONNECT),
	RM_CONNECTION_TYPE_SOFTPHONE  = 0x80,
} RmConnectionType;

typedef struct RmConnection RmConnection;

/**
 * RmConnection:
 *
 * The #RmConnection-struct contains only private fileds and should not be directly accessed.
 */
struct RmConnection {
	/*< private >*/
	/* Device owning this connection */
	gpointer device;
	/* Unique ID */
	guint id;
	/* Type */
	guchar type;
	/* Local number */
	gchar *local_number;
	/* Remote number */
	gchar *remote_number;
	/* Connection time */
	GTimer *duration_timer;
	/* Private data */
	void *priv;
};

RmConnection *rm_connection_add(gpointer device, gint id, RmConnectionType type, const gchar *local_number, const gchar *remote_number);
RmConnection *rm_connection_find_by_id(gint id);
void rm_connection_set_type(RmConnection *connection, RmConnectionType type);
void rm_connection_remove(RmConnection *connection);
gchar *rm_connection_get_duration_time(RmConnection *connection);
void rm_connection_shutdown_duration_timer(RmConnection *connection);

G_END_DECLS

#endif
