/*
 * The rm project
 * Copyright (c) 2012-2020 Jan-Michael Brummer
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

#pragma once

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <glib.h>

#include <rm/rmconnection.h>
#include <rm/rmnetwork.h>
#include <rm/rmcontact.h>

G_BEGIN_DECLS

void rm_object_emit_connection_changed(gint event, RmConnection *connection);
void rm_object_emit_contact_process(RmContact *contact);
void rm_object_emit_fax_process(const gchar *filename);
void rm_object_emit_connection_incoming(RmConnection *connection);
void rm_object_emit_connection_outgoing(RmConnection *connection);
void rm_object_emit_connection_connect(RmConnection *connection);
void rm_object_emit_connection_disconnect(RmConnection *connection);
void rm_object_emit_connection_status(gint status, RmConnection *connection);
void rm_object_emit_message(gchar *title, gchar *message);
void rm_object_emit_contacts_changed(void);
void rm_object_emit_authenticate(RmAuthData *auth_data);
void rm_object_emit_profile_changed (void);

G_END_DECLS

