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

#ifndef FIRMWARE_COMMON_H
#define FIRMWARE_COMMON_H

#include <string.h>

#include <rm/rm.h>

#include "fritzbox.h"

G_BEGIN_DECLS

#define FIRMWARE_IS(major, minor) (((profile->router_info->maj_ver_id == major) && (profile->router_info->min_ver_id >= minor)) || (profile->router_info->maj_ver_id > major))

enum fritzbox_phone_ports {
	PORT_SOFTPHONE,
	PORT_ANALOG1,
	PORT_ANALOG2,
	PORT_ANALOG3,
	PORT_ISDNALL,
	PORT_ISDN1,
	PORT_ISDN2,
	PORT_ISDN3,
	PORT_ISDN4,
	PORT_ISDN5,
	PORT_ISDN6,
	PORT_ISDN7,
	PORT_ISDN8,
	PORT_DECT1,
	PORT_DECT2,
	PORT_DECT3,
	PORT_DECT4,
	PORT_DECT5,
	PORT_DECT6,
	PORT_IP1,
	PORT_IP2,
	PORT_IP3,
	PORT_IP4,
	PORT_IP5,
	PORT_IP6,
	PORT_IP7,
	PORT_IP8,
	PORT_IP9,
	PORT_IP10,
	PORT_MAX
};

struct voice_data {
	/* 0 */
	gint header;
	/* 4 */
	gint index;
	/* 8 (2=own message, 3=remote message) */
	gint type;
	/* 12 */
	guint sub_type;
	/* 16 */
	guint size;
	/* 20 */
	guint duration;
	/* 24 */
	guint status;
	/* 28 */
	guchar tmp0[24];
	/* 52 */
	gchar remote_number[54];
	/* 106 */
	gchar tmp1[18];
	/* 124 */
	gchar file[32];
	/* 151 */
	gchar path[128];
	/* 279 */
	guchar day;
	guchar month;
	guchar year;
	guchar hour;
	guchar minute;
	guchar tmp2[31];
	gchar local_number[24];
	gchar tmp3[4];
};

struct voice_box {
	gsize len;
	gpointer data;
};

extern FritzBoxPhonePort fritzbox_phone_ports[PORT_MAX];

gchar **xml_extract_tags(const gchar *data, gchar *tag_start, gchar *tag_end);
gchar *xml_extract_input_value(const gchar *data, gchar *tag);
gchar *xml_extract_input_value_r(const gchar *data, gchar *tag);
gchar *xml_extract_list_value(const gchar *data, gchar *tag);
gchar *html_extract_assignment(const gchar *data, gchar *tag, gboolean p);
gchar **strv_remove_duplicates(gchar **numbers);
gboolean fritzbox_present(RmRouterInfo *router_info);
gboolean fritzbox_logout(RmProfile *profile, gboolean force);
void fritzbox_read_msn(RmProfile *profile, const gchar *data);
gint fritzbox_get_dialport(gint type);
gchar *fritzbox_load_fax(RmProfile *profile, const gchar *filename, gsize *len);
gchar *fritzbox_load_voice(RmProfile *profile, const gchar *filename, gsize *len);
GList *fritzbox_load_voicebox(GList *journal);
GList *fritzbox_load_faxbox(GList *journal);
gint fritzbox_find_phone_port(gint dial_port);
gchar *fritzbox_get_ip(RmProfile *profile);
gboolean fritzbox_reconnect(RmProfile *profile);
gboolean fritzbox_delete_fax(RmProfile *profile, const gchar *filename);
gboolean fritzbox_delete_voice(RmProfile *profile, const gchar *filename);
gboolean strv_contains(const gchar *const *strv, const gchar *str);

/**
 * make_dots:
 * @str: UTF8 string
 *
 * Make dots (UTF8 -> UTF16)
 *
 * Returns: UTF16 string
 */
static inline gchar *make_dots(const gchar *str)
{
	GString *new_str = g_string_new("");
	gunichar chr;
	gchar *next;

	while (str && *str) {
		chr = g_utf8_get_char(str);
		next = g_utf8_next_char(str);

		if (chr > 255) {
			new_str = g_string_append_c(new_str, '.');
		} else {
			new_str = g_string_append_c(new_str, chr);
		}

		str = next;
	}

	return g_string_free(new_str, FALSE);
}

/**
 * md5:
 * @input: input string
 *
 * Compute md5 sum of input string
 *
 * Returns: md5 in hex or %NULL on error
 */
static inline gchar *md5(gchar *input)
{
	GError *error = NULL;
	gchar *ret = NULL;
	gsize written;
	gchar *bin = g_convert(input, -1, "UTF-16LE", "UTF-8", NULL, &written, &error);

	if (error == NULL) {
		ret = g_compute_checksum_for_string(G_CHECKSUM_MD5, (gchar*)bin, written);
		g_free(bin);
	} else {
		g_debug("Error converting utf8 to utf16: '%s'", error->message);
		g_error_free(error);
	}

	return ret;
}

G_END_DECLS

#endif
