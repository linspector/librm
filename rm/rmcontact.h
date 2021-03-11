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

#ifndef __RM_CONTACT_H__
#define __RM_CONTACT_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <gio/gio.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

/**
 * RmContactAddress:
 *
 * The #RmContactAddress-struct contains only private fileds and should not be directly accessed.
 */
typedef struct {
	/*< private >*/
	gint type;
	gchar *street;
	gchar *zip;
	gchar *city;
	gboolean lookup;
} RmContactAddress;

/**
 * RmContact:
 *
 * The #RmContact-struct contains only private fileds and should not be directly accessed.
 */
struct _RmContact {
	GObject parent_instance;

	/*< private >*/
	/* Name */
	gchar *name;
	/* Picture */
	GdkPixbuf *image;
	/* Company */
	gchar *company;

	/* Phone numbers */
	GList *numbers;
	/* Addresses */
	GList *addresses;

	/* currently active number */
	gchar *number;

	/* Identified data based on active number */
	gchar *street;
	gchar *zip;
	gchar *city;
	gboolean lookup;

	/* Private data */
	gpointer priv;
};

#define RM_TYPE_CONTACT (rm_contact_get_type())

G_DECLARE_FINAL_TYPE(RmContact, rm_contact, RM, CONTACT, GObject)

void rm_contact_copy(RmContact *src, RmContact *dst);
RmContact *rm_contact_dup(RmContact *src);
gint rm_contact_name_compare(gconstpointer a, gconstpointer b);
RmContact *rm_contact_find_by_number(gchar *number);
void rm_contact_free(RmContact *contact);
void rm_contact_set_image_from_file(RmContact *contact, gchar *file);

G_END_DECLS

#endif
