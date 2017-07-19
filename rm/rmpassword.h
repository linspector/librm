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

#ifndef __RM_PASSWORD_H__
#define __RM_PASSWORD_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <glib.h>

#include <rm/rmprofile.h>

G_BEGIN_DECLS

/**
 * RmPasswordManager:
 *
 * The #RmPasswordManager-struct contains only private fileds and should not be directly accessed.
 */
typedef struct {
	/*< private >*/
	const gchar *name;
	void (*set)(RmProfile *profile, const gchar *name, const gchar *password);
	gchar *(*get)(RmProfile *profile, const gchar *name);
	gboolean (*remove)(RmProfile *profile, const gchar *name);
} RmPasswordManager;

void rm_password_set(RmProfile *profile, const gchar *name, const gchar *password);
gchar *rm_password_get(RmProfile *profile, const gchar *name);
gboolean rm_password_remove(RmProfile *profile, const gchar *name);
void rm_password_register(RmPasswordManager *manager);
GSList *rm_password_get_plugins(void);
gchar *rm_password_encode(const gchar *in);
guchar *rm_password_decode(const gchar *in);

G_END_DECLS

#endif
