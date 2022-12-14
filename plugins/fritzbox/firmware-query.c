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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <glib.h>
#include <json-glib/json-glib.h>

#include <rm/rm.h>

#include "fritzbox.h"
#include "firmware-common.h"
#include "firmware-query.h"

/**
 * fritzbox_get_settings_query:
 * @profile: a #RmProfile
 *
 * Extract settings using query lua
 *
 * Returns: %TRUE on success
 */
gboolean fritzbox_get_settings_query(RmProfile *profile)
{
	JsonParser *parser;
	JsonReader *reader;
	SoupMessage *msg;
	const gchar *data;
	gsize read;
	gchar *url;
	gchar *scramble;
	gint i;

	g_test_timer_start();
	/* Login */
	if (!rm_router_login(profile)) {
		g_debug("%s(): Failed to log on", __FUNCTION__);

		return FALSE;
	}


	/* Extract data */
	url = g_strdup_printf("http://%s/query.lua", rm_router_get_host(profile));
	msg = soup_form_request_new(SOUP_METHOD_GET, url,
				    "LKZPrefix", "telcfg:settings/Location/LKZPrefix",
				    "LKZ", "telcfg:settings/Location/LKZ",
				    "OKZPrefix", "telcfg:settings/Location/OKZPrefix",
				    "OKZ", "telcfg:settings/Location/OKZ",
				    "Port0", "telcfg:settings/MSN/Port0/Name",
				    "Port1", "telcfg:settings/MSN/Port1/Name",
				    "Port2", "telcfg:settings/MSN/Port2/Name",
				    "TAM", "tam:settings/TAM/list(Name)",
				    "ISDNName0", "telcfg:settings/NTHotDialList/Name0",
				    "ISDNName1", "telcfg:settings/NTHotDialList/Name1",
				    "ISDNName2", "telcfg:settings/NTHotDialList/Name2",
				    "ISDNName3", "telcfg:settings/NTHotDialList/Name3",
				    "ISDNName4", "telcfg:settings/NTHotDialList/Name4",
				    "ISDNName5", "telcfg:settings/NTHotDialList/Name5",
				    "ISDNName6", "telcfg:settings/NTHotDialList/Name6",
				    "ISDNName7", "telcfg:settings/NTHotDialList/Name7",
				    "DECT", "telcfg:settings/Foncontrol/User/list(Name,Type,Intern)",
				    "MSN", "telcfg:settings/MSN/list(MSN,Name)",
				    "FaxMailActive", "telcfg:settings/FaxMailActive",
				    "storage", "ctlusb:settings/storage-part0",
				    "FaxMSN0", "telcfg:settings/FaxMSN0",
				    "FaxKennung", "telcfg:settings/FaxKennung",
				    "DialPort", "telcfg:settings/DialPort",
				    "TamStick", "tam:settings/UseStick",
				    "SIP", "telcfg:settings/SIP/list(MSN,Name)",
				    "SIP2", "sip:settings/sip/list(activated,displayname,registrar,outboundproxy,providername,ID,gui_readonly,webui_trunk_id,msn)",
				    "IPP", "telcfg:settings/VoipExtension/list(Name,Number)",
				    "FON", "telcfg:settings/Foncontrol/User/list(Name,Type,Intern)",
				    "Journal", "telcfg:settings/list(Journal)",
				    "J", "telcfg:settings/Journal/listwindow(0,6,Type,Date,Number,Port,Duration,Route,RouteType,Name,NumberType,PortName)",
				    "sid", profile->router_info->session_id,
				    NULL);
	g_free(url);

	soup_session_send_message(rm_soup_session, msg);
	if (msg->status_code != 200) {
		g_debug("%s(): Received status code: %d (%s)", __FUNCTION__, msg->status_code, soup_status_get_phrase(msg->status_code));
		g_object_unref(msg);

		fritzbox_logout(profile, FALSE);
		return FALSE;
	}
	data = msg->response_body->data;
	read = msg->response_body->length;

	rm_log_save_data("fritzbox-06_35-query.html", data, read);
	g_assert(data != NULL);

	parser = json_parser_new();
	json_parser_load_from_data(parser, data, read, NULL);

	reader = json_reader_new(json_parser_get_root(parser));

	json_reader_read_member(reader, "LKZ");
	const gchar *lkz = json_reader_get_string_value(reader);
	g_debug("%s(): LKZ: %s", __FUNCTION__, lkz);
	g_settings_set_string(profile->settings, "country-code", lkz);
	json_reader_end_member(reader);

	json_reader_read_member(reader, "LKZPrefix");
	const gchar *lkz_prefix = json_reader_get_string_value(reader);
	g_debug("%s(): LKZPrefix: %s", __FUNCTION__, lkz_prefix);
	g_settings_set_string(profile->settings, "international-access-code", lkz_prefix);
	json_reader_end_member(reader);

	json_reader_read_member(reader, "OKZ");
	const gchar *okz = json_reader_get_string_value(reader);
	g_debug("%s(): OKZ: %s", __FUNCTION__, okz);
	g_settings_set_string(profile->settings, "area-code", okz);
	json_reader_end_member(reader);

	json_reader_read_member(reader, "OKZPrefix");
	const gchar *okz_prefix = json_reader_get_string_value(reader);
	g_debug("%s(): OKZPrefix: %s", __FUNCTION__, okz_prefix);
	g_settings_set_string(profile->settings, "national-access-code", okz_prefix);
	json_reader_end_member(reader);

	json_reader_read_member(reader, "FaxMailActive");
	const gchar *port_str = json_reader_get_string_value(reader);
	gint port = atoi(port_str);
	g_debug("%s(): FaxMailActive: %d", __FUNCTION__, port);
	json_reader_end_member(reader);

	json_reader_read_member(reader, "FaxKennung");
	const gchar *fax_ident = json_reader_get_string_value(reader);
	scramble = rm_number_scramble(fax_ident);
	g_debug("%s(): FaxKennung: %s", __FUNCTION__, scramble);
	g_free(scramble);
	g_settings_set_string(profile->settings, "fax-header", fax_ident);
	json_reader_end_member(reader);

	json_reader_read_member(reader, "storage");
	const gchar *storage = json_reader_get_string_value(reader);
	g_debug("%s(): Fax Storage: %s", __FUNCTION__, storage);
	g_settings_set_string(fritzbox_settings, "fax-volume", storage);
	json_reader_end_member(reader);

	json_reader_read_member(reader, "FaxMSN0");
	const gchar *fax_msn = json_reader_get_string_value(reader);
	scramble = rm_number_scramble(fax_msn);
	g_debug("%s(): FaxMSN0: %s", __FUNCTION__, scramble);
	g_free(scramble);
	g_settings_set_string(fritzbox_settings, "fax-number", fax_msn);
	g_settings_set_string(profile->settings, "fax-number", fax_msn);
	json_reader_end_member(reader);

	gchar *formated_number = rm_number_format(profile, fax_msn, RM_NUMBER_FORMAT_INTERNATIONAL_PLUS);
	g_settings_set_string(profile->settings, "fax-ident", formated_number);
	g_free(formated_number);

	/* Parse phones */
	g_debug("%s(): POTS", __FUNCTION__);
	for (i = 0; i < 3; i++) {
		gchar name_in[11];
		gchar name_analog[13];
		const gchar *name;

		memset(name_in, 0, sizeof(name_in));
		g_snprintf(name_in, sizeof(name_in), "Port%d", i);

		json_reader_read_member(reader, name_in);
		name = json_reader_get_string_value(reader);
		g_debug("%s():  %s = %s", __FUNCTION__, name_in, name);

		memset(name_analog, 0, sizeof(name_analog));
		g_snprintf(name_analog, sizeof(name_analog), "name-analog%d", i + 1);
		g_settings_set_string(fritzbox_settings, name_analog, name);
		json_reader_end_member(reader);
	}

	g_debug("%s(): ISDN", __FUNCTION__);
	for (i = 0; i < 8; i++) {
		gchar name_in[11];
		gchar name_isdn[13];
		const gchar *name;

		memset(name_in, 0, sizeof(name_in));
		g_snprintf(name_in, sizeof(name_in), "ISDNName%d", i);

		json_reader_read_member(reader, name_in);
		name = json_reader_get_string_value(reader);
		g_debug("%s():  %s = %s", __FUNCTION__, name_in, name);

		memset(name_isdn, 0, sizeof(name_isdn));
		g_snprintf(name_isdn, sizeof(name_isdn), "name-isdn%d", i + 1);
		g_settings_set_string(fritzbox_settings, name_isdn, name);
		json_reader_end_member(reader);
	}

	g_debug("%s(): DECTs:", __FUNCTION__);
	json_reader_read_member(reader, "DECT");
	gint count = json_reader_count_elements(reader);

	for (i = 1; i < count; i++) {
		const gchar *tmp;
		const gchar *intern;
		gchar name_dect[11];

		json_reader_read_element(reader, i);
		json_reader_read_member(reader, "Name");
		tmp = json_reader_get_string_value(reader);
		g_debug("%s():  Name: %s", __FUNCTION__, tmp);
		json_reader_end_member(reader);

		json_reader_read_member(reader, "Intern");
		intern = json_reader_get_string_value(reader);
		g_debug("%s():  Intern: %s", __FUNCTION__, intern);
		json_reader_end_member(reader);

		memset(name_dect, 0, sizeof(name_dect));
		g_snprintf(name_dect, sizeof(name_dect), "name-dect%d", i);
		g_settings_set_string(fritzbox_settings, name_dect, tmp);

		json_reader_end_element(reader);
	}

	json_reader_end_member(reader);

	/* Parse msns */
	g_debug("%s(): MSNs:", __FUNCTION__);
	json_reader_read_member(reader, "SIP");
	count = json_reader_count_elements(reader);

	gchar **numbers = NULL;
	gint phones = 0;

	for (i = 0; i < count; i++) {
		const gchar *tmp;

		json_reader_read_element(reader, i);
		json_reader_read_member(reader, "MSN");
		tmp = json_reader_get_string_value(reader);
		json_reader_end_member(reader);

		if (!RM_EMPTY_STRING(tmp)) {
			scramble = rm_number_scramble(tmp);
			g_debug("%s():  MSN: %s", __FUNCTION__, scramble);
			g_free(scramble);
			phones++;
			numbers = g_realloc(numbers, (phones + 1) * sizeof(char*));
			numbers[phones - 1] = g_strdup(tmp);
			numbers[phones] = NULL;

			json_reader_read_member(reader, "Name");
			tmp = json_reader_get_string_value(reader);
			g_debug("%s():  Name: %s", __FUNCTION__, tmp);
			json_reader_end_member(reader);
		}

		json_reader_end_element(reader);
	}
	g_settings_set_strv(profile->settings, "numbers", (const gchar*const*)numbers);

	json_reader_end_member(reader);

	json_reader_read_member(reader, "DialPort");
	const gchar *dialport = json_reader_get_string_value(reader);
	port = atoi(dialport);
	gint phone_port = fritzbox_find_phone_port(port);
	g_debug("%s(): Dial port: %s, phone_port: %d", __FUNCTION__, dialport, phone_port);
	g_settings_set_uint(fritzbox_settings, "port", phone_port);
	json_reader_end_member(reader);

	json_reader_read_member(reader, "TamStick");
	const gchar *tam_stick = json_reader_get_string_value(reader);
	g_debug("%s(): TamStick: %s", __FUNCTION__, tam_stick);
	if (tam_stick && atoi(&tam_stick[0])) {
		g_settings_set_uint(fritzbox_settings, "tam-stick", atoi(tam_stick));
	} else {
		g_settings_set_uint(fritzbox_settings, "tam-stick", 0);
	}
	json_reader_end_member(reader);


	g_object_unref(reader);
	g_object_unref(parser);

	g_object_unref(msg);
	g_debug("%s(): Execution time: %f", __FUNCTION__, g_test_timer_elapsed());

	/* The end - exit */
	fritzbox_logout(profile, FALSE);

	return TRUE;
}
