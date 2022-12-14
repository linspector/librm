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

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include <rm/rm.h>

#include "csv.h"
#include "firmware-common.h"

/**
 * csv_parse_fritzbox:
 * @ptr: pointer to journal
 * @split: splitted line
 *
 * Parse FRITZ!Box "Anruferliste"
 *
 * Returns: pointer to journal with attached call line
 */
static inline gpointer csv_parse_fritzbox(gpointer ptr, gchar **split)
{
	GList *list = ptr;

	if (g_strv_length(split) == 7) {
		RmCallEntry *call;
		gint call_type = 0;

		switch (atoi(split[0])) {
		case 1:
			call_type = RM_CALL_ENTRY_TYPE_INCOMING;
			break;
		case 2:
			call_type = RM_CALL_ENTRY_TYPE_MISSED;
			break;
		case 3: {
			RmProfile *profile = rm_profile_get_active();

			if (FIRMWARE_IS(4, 74)) {
				call_type = RM_CALL_ENTRY_TYPE_BLOCKED;
			} else {
				call_type = RM_CALL_ENTRY_TYPE_OUTGOING;
			}
			break;
		}
		case 4:
			call_type = RM_CALL_ENTRY_TYPE_OUTGOING;
			break;
		case 10:
			call_type = RM_CALL_ENTRY_TYPE_BLOCKED;
			break;
		default:
			break;
		}

		call = rm_call_entry_new(call_type, split[1], split[2], split[3], split[4], split[5], split[6], NULL);
		list = rm_journal_add_call_entry(list, call);
	}

	return list;
}

/**
 * csv_parse_fritzbox_journal_data:
 * @list: journal as list
 * @data: raw data to parse
 *
 * Parse journal data as csv
 *
 * Returns: call list
 */
GList *csv_parse_fritzbox_journal_data(GList *list, const gchar *data)
{
	GList *new_list = NULL;

	new_list = rm_csv_parse_data(data, CSV_FRITZBOX_JOURNAL_DE, csv_parse_fritzbox, list);
	if (!new_list) {
		new_list = rm_csv_parse_data(data, CSV_FRITZBOX_JOURNAL_EN, csv_parse_fritzbox, list);
		if (!new_list) {
			new_list = rm_csv_parse_data(data, CSV_FRITZBOX_JOURNAL_EN2, csv_parse_fritzbox, list);
			if (!new_list) {
				new_list = rm_csv_parse_data(data, CSV_FRITZBOX_JOURNAL_EN3, csv_parse_fritzbox, list);
			}
		}
	}

	if (!new_list) {
		rm_log_save_data("fritzbox-journal.csv", data, strlen(data));
	}

	/* Return call list */
	return new_list;
}
