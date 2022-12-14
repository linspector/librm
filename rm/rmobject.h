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

#ifndef __RM_OBJECT_H__
#define __RM_OBJECT_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>

#include <rm/rmconnection.h>
#include <rm/rmcontact.h>
#include <rm/rmnetwork.h>

G_BEGIN_DECLS

/**
 * RmCallbackId
 * @RM_ACB_CONNECTION_CHANGED: connection-changed
 * @RM_ACB_CONTACT_PROCESS: contact-process
 * @RM_ACB_FAX_PROCESS: fax-process
 * @RM_ACB_CONNECTION_STATUS: connection-status
 * @RM_ACB_MESSAGE: message
 * @RM_ACB_CONTACTS_CHANGED: contacts-changed
 * @RM_ACB_AUTHENTICATE: authenticate
 * @RM_ACB_PROFILE_CHANGED: profile-changed
 * @RM_ACB_MAX: Max Id
 *
 * RM Callback signal ids
 */
typedef enum {
	RM_ACB_CONNECTION_CHANGED,
	RM_ACB_CONTACT_PROCESS,
	RM_ACB_FAX_PROCESS,
	RM_ACB_CONNECTION_STATUS,
	RM_ACB_MESSAGE,
	RM_ACB_CONTACTS_CHANGED,
	RM_ACB_AUTHENTICATE,
  RM_ACB_PROFILE_CHANGED,
	RM_ACB_MAX
} RmCallbackId;

#define RM_OBJECT_TYPE (rm_object_get_type())
#define RM_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GEANY_OBJECT_TYPE, RmObject))
#define RM_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), RM_OBJECT_TYPE, RmObjectClass))
#define IS_RM_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), RM_OBJECT_TYPE))
#define IS_RM_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RM_OBJECT_TYPE))

/**
 * RmObject:
 *
 * The #RmObject-struct contains only private fileds and should not be directly accessed.
 */
typedef struct _RmObject {
	/*< private >*/
	GObject parent;
} RmObject;

/**
 * RmObjectClass:
 *
 * The #RmObjectClass-struct contains only private fileds and should not be directly accessed.
 */
typedef struct _RmObjectClass {
	/*< private >*/
	GObjectClass parent_class;
	void (*journal_loaded)(GSList *journal);
	void (*connection_changed)(gint event, RmConnection *connection);
	void (*contact_process)(RmContact *contact);
	void (*fax_process)(const gchar *filename);
	void (*connection_status)(gint status, RmConnection *connection);
	void (*message)(gpointer title, gpointer message);
	void (*contacts_changed)(void);
	void (*authenticate)(RmAuthData *auth_data);
  void (*profile_changed)(void);
} RmObjectClass;

GObject *rm_object_new(void);

extern GObject *rm_object;
extern guint rm_object_signals[RM_ACB_MAX];

G_END_DECLS

#endif
