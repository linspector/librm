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

#include <rm/rmmain.h>
#include <rm/rmutils.h>

/**
 * rm_utils_xml_extract_tag:
 * @data: data to parse
 * @tag: tag to extract
 *
 * Extract XML Tag: <TAG>VALUE</TAG>
 *
 * Returns: tag value
 */
gchar *rm_utils_xml_extract_tag(const gchar *data, gchar *tag)
{
	gchar *regex_str = g_strdup_printf("<%s>[^<]*</%s>", tag, tag);
	GRegex *regex = NULL;
	GError *error = NULL;
	GMatchInfo *match_info;
	gchar *entry = NULL;
	glong tag_len = strlen(tag);

	regex = g_regex_new(regex_str, 0, 0, &error);
	g_assert(regex != NULL);

	g_regex_match(regex, data, 0, &match_info);

	while (match_info && g_match_info_matches(match_info)) {
		gint start;
		gint end;
		gboolean fetched = g_match_info_fetch_pos(match_info, 0, &start, &end);

		if (fetched == TRUE) {
			gint entry_size = end - start - 2 * tag_len - 5;
			entry = g_malloc0(entry_size + 1);
			strncpy(entry, data + start + tag_len + 2, entry_size);
			break;
		}

		if (g_match_info_next(match_info, NULL) == FALSE) {
			break;
		}
	}

	g_match_info_free(match_info);
	g_free(regex_str);

	return entry;
}
