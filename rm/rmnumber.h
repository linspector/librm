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

#ifndef __RM_NUMBER_H__
#define __RM_NUMBER_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <rm/rmcontact.h>
#include <rm/rmprofile.h>

G_BEGIN_DECLS

/**
 * RmNumberFormats:
 * @RM_NUMBER_FORMAT_UNKNOWN: Unknown format
 * @RM_NUMBER_FORMAT_LOCAL: Local format
 * @RM_NUMBER_FORMAT_NATIONAL: National format
 * @RM_NUMBER_FORMAT_INTERNATIONAL: International format
 * @RM_NUMBER_FORMAT_INTERNATIONAL_PLUS: International format with a + as prefix
 *
 * Format of phone number.
 */
typedef enum rm_number_formats {
	RM_NUMBER_FORMAT_UNKNOWN,
	RM_NUMBER_FORMAT_LOCAL,
	RM_NUMBER_FORMAT_NATIONAL,
	RM_NUMBER_FORMAT_INTERNATIONAL,
	RM_NUMBER_FORMAT_INTERNATIONAL_PLUS
} RmNumberFormats;

/**
 * RmCallByCallEntry:
 *
 * The #RmCallByCallEntry-struct contains only private fileds and should not be directly accessed.
 */
typedef struct {
	/*< private >*/
	gchar *country_code;
	gchar *prefix;
	gint prefix_length;
} RmCallByCallEntry;

gchar *rm_number_scramble(const gchar *number);
gchar *rm_number_full(const gchar *number, gboolean country_code_prefix);
gchar *rm_number_format(RmProfile *profile, const gchar *number, RmNumberFormats output_format);
gchar *rm_number_canonize(const gchar *number);

G_END_DECLS

#endif
