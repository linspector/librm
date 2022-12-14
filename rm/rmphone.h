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

#ifndef __RM_PHONE_H__
#define __RM_PHONE_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <glib.h>

#include <rm/rmconnection.h>
#include <rm/rmdevice.h>
#include <rm/rmphone.h>

G_BEGIN_DECLS

typedef struct _RmPhone RmPhone;

/**
 * RmPhone:
 *
 * The #RmPhone-struct contains only private fileds and should not be directly accessed.
 */
struct _RmPhone {
	/*< private >*/
	RmDevice *device;
	gchar *name;
	RmConnection *(*dial)(RmPhone *phone, const gchar *target, gboolean anonymous);
	gint (*pickup)(RmConnection *connection);
	void (*hangup)(RmConnection *connection);
	void (*hold)(RmConnection *connection, gboolean hold);
	void (*send_dtmf_code)(RmConnection *connection, guchar code);
	void (*mute)(RmConnection *connection, gboolean mute);
	void (*record)(RmConnection *connection, gboolean record);

	/* Private information for phone plugins, e.g. internal connection or status information */
	gpointer priv;
};

/**
 * RM_PHONE:
 * @x: a #RmPhone
 *
 * Cast x to #RmPhone
 */
#define RM_PHONE(x) (RmPhone*)(x)

void rm_phone_register(RmPhone *phone);
void rm_phone_unregister(RmPhone *phone);

GList *rm_phone_get_plugins(void);
RmPhone *rm_phone_get(const gchar *name);
gchar *rm_phone_get_name(RmPhone *phone);

void rm_phone_mute(RmPhone *phone, RmConnection *connection, gboolean mute);
void rm_phone_record(RmPhone *phone, RmConnection *connection, gboolean record);
void rm_phone_hold(RmPhone *phone, RmConnection *connection, gboolean hold);
void rm_phone_dtmf(RmPhone *phone, RmConnection *connection, guchar code);
void rm_phone_hangup(RmConnection *connection);
gint rm_phone_pickup(RmConnection *connection);
RmConnection *rm_phone_dial(RmPhone *phone, const gchar *target, gboolean anonymous);

G_END_DECLS

#endif
