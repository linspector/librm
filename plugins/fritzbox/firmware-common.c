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
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <glib.h>

#include <libsoup/soup.h>

#include <rm/rm.h>

#include "fritzbox.h"
#include "firmware-common.h"
#include "firmware-04-00.h"
#include "firmware-tr64.h"

static struct voice_box voice_boxes[5];

/**
 * \brief Extract XML Tags: <TAG>VALUE</TAG>
 * \param data data to parse
 * \param tag tag to extract
 * \return tag values
 */
gchar **xml_extract_tags(const gchar *data, gchar *tag_start, gchar *tag_end)
{
	gchar *regex_str = g_strdup_printf("<%s>[^<]*</%s>", tag_start, tag_end);
	GRegex *regex = NULL;
	GError *error = NULL;
	GMatchInfo *match_info;
	gchar **entries = NULL;
	gint index = 0;

	regex = g_regex_new(regex_str, 0, 0, &error);
	g_assert(regex != NULL);

	g_regex_match(regex, data, 0, &match_info);

	while (match_info && g_match_info_matches(match_info)) {
		gint start;
		gint end;
		gboolean fetched = g_match_info_fetch_pos(match_info, 0, &start, &end);

		if (fetched == TRUE) {
			gchar *tag_start_pos = strchr(data + start, '>');
			gchar *tag_end_pos = strchr(tag_start_pos + 1, '<');
			gint entry_size = tag_end_pos - tag_start_pos - 1;

			entries = g_realloc(entries, (index + 2) * sizeof(gchar *));
			entries[index] = g_malloc0(entry_size + 1);
			strncpy(entries[index], tag_start_pos + 1, entry_size);
			entries[index + 1] = NULL;
			index++;
		}

		if (g_match_info_next(match_info, NULL) == FALSE) {
			break;
		}
	}

	g_match_info_free(match_info);
	g_free(regex_str);

	return entries;
}

/**
 * \brief Extract XML input tag: name="TAG" ... value="VALUE"
 * \param data data to parse
 * \param tag tag to extract
 * \return tag value
 */
gchar *xml_extract_input_value(const gchar *data, gchar *tag)
{
	gchar *name = g_strdup_printf("name=\"%s\"", tag);
	gchar *start = g_strstr_len(data, -1, name);
	gchar *val_start = NULL;
	gchar *val_end = NULL;
	gchar *value = NULL;
	gssize val_size;

	g_free(name);
	if (start == NULL) {
		return NULL;
	}

	val_start = g_strstr_len(start, -1, "value=\"");
	g_assert(val_start != NULL);

	val_start += 7;

	val_end = g_strstr_len(val_start, -1, "\"");

	val_size = val_end - val_start;
	g_assert(val_size >= 0);

	value = g_malloc0(val_size + 1);
	memcpy(value, val_start, val_size);

	return value;
}

/**
 * \brief Extract XML input tag reverse: name="TAG" ... value="VALUE"
 * \param data data to parse
 * \param tag tag to extract
 * \return tag value
 */
gchar *xml_extract_input_value_r(const gchar *data, gchar *tag)
{
	gchar *name = g_strdup_printf("name=\"%s\"", tag);
	gchar *start = g_strstr_len(data, -1, name);
	gchar *val_start = NULL;
	gchar *val_end = NULL;
	gchar *value = NULL;
	gssize val_size;

	g_free(name);
	if (start == NULL) {
		return NULL;
	}

	val_start = g_strrstr_len(data, start - data, "value=\"");
	g_assert(val_start != NULL);

	val_start += 7;

	val_end = g_strstr_len(val_start, -1, "\"");

	val_size = val_end - val_start;
	g_assert(val_size >= 0);

	value = g_malloc0(val_size + 1);
	memcpy(value, val_start, val_size);

	return value;
}

/**
 * \brief Extract XML list tag: ["TAG"] = "VALUE"
 * \param data data to parse
 * \param tag tag to extract
 * \return tag value
 */
gchar *xml_extract_list_value(const gchar *data, gchar *tag)
{
	gchar *name = g_strdup_printf("\"%s\"", tag);
	gchar *start = g_strstr_len(data, -1, name);
	gchar *val_start = NULL;
	gchar *val_end = NULL;
	gchar *value = NULL;
	gssize val_size;

	g_free(name);
	if (start == NULL) {
		return NULL;
	}

	start += strlen(tag) + 2;

	val_start = g_strstr_len(start, -1, "\"");
	g_assert(val_start != NULL);

	val_start += 1;

	val_end = g_strstr_len(val_start, -1, "\"");

	val_size = val_end - val_start;
	g_assert(val_size >= 0);

	value = g_malloc0(val_size + 1);
	memcpy(value, val_start, val_size);

	return value;
}

/**
 * \brief Extract HTML assignment: TAG = "VALUE"
 * \param data data to parse
 * \param tag tag to extract
 * \param p value is surrounded by " flag
 * \return tag value
 */
gchar *html_extract_assignment(const gchar *data, gchar *tag, gboolean p)
{
	gchar *name = g_strdup_printf("%s", tag);
	gchar *start = g_strstr_len(data, -1, name);
	gchar *val_start = NULL;
	gchar *val_end = NULL;
	gchar *value = NULL;
	gssize val_size;

	g_free(name);
	if (start == NULL) {
		return NULL;
	}

	start += strlen(tag);

	if (p == 1) {
		start += 2;
		val_start = g_strstr_len(start, -1, "\"");
		g_assert(val_start != NULL);

		val_start += 1;

		val_end = g_strstr_len(val_start, -1, "\"");

		val_size = val_end - val_start;
		g_assert(val_size >= 0);
	} else {
		val_start = start;
		g_assert(val_start != NULL);

		val_start += 1;

		val_end = g_strstr_len(val_start, -1, "\n");

		val_size = val_end - val_start - 2;
		g_assert(val_size >= 0);
	}

	value = g_malloc0(val_size + 1);
	memcpy(value, val_start, val_size);

	return value;
}


/**
 * \brief Remove duplicate entries from string array
 * \param numbers input string array
 * \return duplicate free string array
 */
gchar **strv_remove_duplicates(gchar **numbers)
{
	gchar **ret = NULL;
	gint len = g_strv_length(numbers);
	gint idx;
	gint ret_idx = 1;

	for (idx = 0; idx < len; idx++) {
		if (!ret || !rm_strv_contains((const gchar*const*)ret, numbers[idx])) {
			ret = g_realloc(ret, (ret_idx + 1) * sizeof(char *));
			ret[ret_idx - 1] = g_strdup(numbers[idx]);
			ret[ret_idx] = NULL;

			ret_idx++;
		}
	}

	return ret;
}


/**
 * \brief Check if a FRITZ!Box router is present
 * \param router_info - router information structure
 * \return true if fritzbox and type could be retrieved, otherwise false
 */
gboolean fritzbox_present(RmRouterInfo *router_info)
{
	SoupMessage *msg;
	gsize read;
	const gchar *data;
	gchar *name;
	gchar *version;
	gchar *lang;
	gchar *serial;
	gchar *url;
	gchar *annex;
	gboolean ret = FALSE;

	if (router_info->name != NULL) {
		g_free(router_info->name);
	}

	if (router_info->version != NULL) {
		g_free(router_info->version);
	}

	if (router_info->session_timer != NULL) {
		//g_timer_destroy(router_info->session_timer);
		router_info->session_timer = NULL;
	}

	url = g_strdup_printf("http://%s/jason_boxinfo.xml", router_info->host);
	msg = soup_message_new(SOUP_METHOD_GET, url);

	soup_session_send_message(rm_soup_session, msg);
	if (msg->status_code != 200) {
		g_object_unref(msg);
		g_free(url);

		if (msg->status_code == 404) {
			ret = fritzbox_present_04_00(router_info);
		} else {
			g_warning("Could not read boxinfo file (Error: %d, %s)", msg->status_code, soup_status_get_phrase(msg->status_code));
		}

		return ret;
	}
	data = msg->response_body->data;
	read = msg->response_body->length;

	rm_log_save_data("fritzbox-present.html", data, read);
	g_return_val_if_fail(data != NULL, FALSE);

	name = rm_utils_xml_extract_tag(data, "j:Name");
	version = rm_utils_xml_extract_tag(data, "j:Version");
	lang = rm_utils_xml_extract_tag(data, "j:Lang");
	serial = rm_utils_xml_extract_tag(data, "j:Serial");
	annex = rm_utils_xml_extract_tag(data, "j:Annex");

	g_object_unref(msg);
	g_free(url);

	if (name && version && lang && serial && annex) {
		gchar **split;

		router_info->name = g_strdup(name);
		router_info->version = g_strdup(version);
		router_info->lang = g_strdup(lang);
		router_info->serial = g_strdup(serial);
		router_info->annex = g_strdup(annex);

		/* Version: Box.Major.Minor(-XXXXX) */

		split = g_strsplit(router_info->version, ".", -1);

		router_info->box_id = atoi(split[0]);
		router_info->maj_ver_id = atoi(split[1]);
		router_info->min_ver_id = atoi(split[2]);

		g_strfreev(split);
		ret = TRUE;
	} else {
		g_warning("name, version, lang or serial not valid");
	}

	g_free(annex);
	g_free(serial);
	g_free(lang);
	g_free(version);
	g_free(name);

	return ret;
}

/**
 * \brief Logout of box (if session_timer is not present)
 * \param profile profile information structure
 * \param force force logout
 * \return error code
 */
gboolean fritzbox_logout(RmProfile *profile, gboolean force)
{
	SoupMessage *msg;
	gchar *url;

	if (profile->router_info->session_timer && !force) {
		return TRUE;
	}

	//url = g_strdup_printf("http://%s/cgi-bin/webcm", router_get_host(profile));
	//msg = soup_form_request_new(SOUP_METHOD_POST, url,
	//                            "sid", profile->router_info->session_id,
	//                            "security:command/logout", "",
	//                            "getpage", "../html/confirm_logout.html",
	//                            NULL);
	url = g_strdup_printf("http://%s/home/home.lua", rm_router_get_host(profile));
	msg = soup_form_request_new(SOUP_METHOD_POST, url,
				    "sid", profile->router_info->session_id,
				    "logout", "1",
				    NULL);
	g_free(url);

	soup_session_send_message(rm_soup_session, msg);
	if (msg->status_code != 200) {
		g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
		g_object_unref(msg);
		return FALSE;
	}

	if (profile->router_info->session_timer != NULL) {
		g_timer_destroy(profile->router_info->session_timer);
		profile->router_info->session_timer = NULL;
	}

	g_object_unref(msg);
	g_debug("%s(): Successful", __FUNCTION__);

	return TRUE;
}

/**
 * \brief Read MSNs of data
 * \param profile profile information structure
 * \param data data to parse for MSNs
 */
void fritzbox_read_msn(RmProfile *profile, const gchar *data)
{
	gchar *func;
	gchar *pots_start;
	gchar *pots_end;
	gchar *pots;
	gint pots_len;

	gchar *msns_start;
	gchar *msns_end;
	gchar *msns;
	gint msns_len;

	gchar *sips_start;
	gchar *sips_end;
	gchar *sips;
	gint sips_len;

	gint index;

	/* Check for readFonNumbers() function */
	func = g_strstr_len(data, -1, "readFonNumbers()");
	if (!func) {
		return;
	}

	/* Extract POTS */
	pots_start = g_strstr_len(func, -1, "nrs.pots");
	g_assert(pots_start != NULL);

	pots_start += 11;

	pots_end = g_strstr_len(pots_start, -1, "\"");
	g_assert(pots_end != NULL);

	pots_len = pots_end - pots_start;

	pots = g_slice_alloc0(pots_len + 1);
	strncpy(pots, pots_start, pots_len);
	if (strlen(pots) > 0) {
		g_debug("pots: '%s'", pots);
	}
	g_slice_free1(pots_len + 1, pots);

	/* Extract MSNs */
	for (index = 0; index < 10; index++) {
		msns_start = g_strstr_len(func, -1, "nrs.msn.push");
		g_assert(msns_start != NULL);

		msns_start += 14;

		msns_end = g_strstr_len(msns_start, -1, "\"");
		g_assert(msns_end != NULL);

		msns_len = msns_end - msns_start;

		msns = g_slice_alloc0(msns_len + 1);
		strncpy(msns, msns_start, msns_len);

		if (strlen(msns) > 0) {
			g_debug("msn%d: '%s'", index, msns);
		}
		g_slice_free1(msns_len + 1, msns);

		func = msns_end;
	}

	/* Extract SIPs */
	for (index = 0; index < 19; index++) {
		sips_start = g_strstr_len(func, -1, "nrs.sip.push");
		g_assert(sips_start != NULL);

		sips_start += 14;

		sips_end = g_strstr_len(sips_start, -1, "\"");
		g_assert(sips_end != NULL);

		sips_len = sips_end - sips_start;

		sips = g_slice_alloc0(sips_len + 1);
		strncpy(sips, sips_start, sips_len);

		if (strlen(sips) > 0) {
			g_debug("sip%d: '%s'", index, sips);
		}
		g_slice_free1(sips_len + 1, sips);

		func = sips_end;
	}
}

/**
 * \brief Depending on type get dialport
 * \param type phone type
 * \return dialport
 */
gint fritzbox_get_dialport(gint type)
{
	gint index;

	for (index = 0; index < PORT_MAX; index++) {
		if (fritzbox_phone_ports[index].type == type) {
			return fritzbox_phone_ports[index].number;
		}
	}

	return -1;
}

/**
 * \brief Load faxbox and add it to journal
 * \param journal journal call list
 * \return journal list with added faxbox
 */
GList *fritzbox_load_faxbox(GList *journal)
{
	RmProfile *profile = rm_profile_get_active();
	RmFtp *client;
	gchar *user = rm_router_get_ftp_user(profile);
	gchar *response;
	gchar *path;
	gchar *volume_path;

	client = rm_ftp_init(rm_router_get_host(profile));
	if (!client) {
		return journal;
	}

	if (!rm_ftp_login(client, user, rm_router_get_ftp_password(profile))) {
		g_warning("Could not login to router ftp");
		rm_object_emit_message(R_("FTP Login failed"), R_("Please check your ftp credentials"));
		rm_ftp_shutdown(client);
		return journal;
	}

	if (!rm_ftp_passive(client)) {
		g_warning("Could not switch to passive mode");
		rm_ftp_shutdown(client);
		return journal;
	}

	volume_path = g_settings_get_string(fritzbox_settings, "fax-volume");
	path = g_build_filename(volume_path, "FRITZ/faxbox/", NULL);
	g_free(volume_path);
	response = rm_ftp_list_dir(client, path);
	if (response) {
		gchar **split;
		gint index;

		split = g_strsplit(response, "\n", -1);

		for (index = 0; index < g_strv_length(split); index++) {
			RmCallEntry *call;
			gchar date[9];
			gchar time[6];
			gchar remote_number[32];
			gchar *start;
			gchar *pos;
			gchar *full;
			gchar *number;

			start = strstr(split[index], "Telefax");
			if (!start) {
				continue;
			}

			full = g_strconcat(path, split[index], NULL);
			strncpy(date, split[index], 8);
			date[8] = '\0';

			strncpy(time, split[index] + 9, 5);
			time[2] = ':';
			time[5] = '\0';

			pos = strstr(start + 8, ".");
			strncpy(remote_number, start + 8, pos - start - 8);
			remote_number[pos - start - 8] = '\0';

			if (isdigit(remote_number[0])) {
				number = remote_number;
			} else {
				number = "";
			}

			call = rm_call_entry_new(RM_CALL_ENTRY_TYPE_FAX, g_strdup_printf("%s %s", date, time), "", number, ("Telefax"), "", "0:01", g_strdup(full));
			journal = rm_journal_add_call_entry(journal, call);
			g_free(full);
		}

		g_strfreev(split);

		g_free(response);
	}
	g_free(path);

	rm_ftp_shutdown(client);

	return journal;
}

/**
 * \brief Parse voice data structure and add calls to journal
 * \param journal journal call list
 * \param data meta data to parse voice data for
 * \param len length of data
 * \return journal call list with voicebox data
 */
static GList *fritzbox_parse_voice_data(GList *journal, const gchar *data, gsize len)
{
	gint index;

	for (index = 0; index < len / sizeof(struct voice_data); index++) {
		RmCallEntry *call;
		struct voice_data *voice_data = (struct voice_data *)(data + index * sizeof(struct voice_data));
		gchar date_time[20];

		/* Skip user/standard welcome message */
		if (!strncmp(voice_data->file, "uvp", 3)) {
			continue;
		}

		if (voice_data->header == 0x5C010000) {
			voice_data->header = GINT_TO_BE(voice_data->header);
			voice_data->type = GINT_TO_BE(voice_data->type);
			voice_data->sub_type = GUINT_TO_BE(voice_data->sub_type);
			voice_data->size = GUINT_TO_BE(voice_data->size);
			voice_data->duration = GUINT_TO_BE(voice_data->duration);
			voice_data->status = GUINT_TO_BE(voice_data->status);
		}

		snprintf(date_time, sizeof(date_time), "%2.2d.%2.2d.%2.2d %2.2d:%2.2d", voice_data->day, voice_data->month, voice_data->year,
			 voice_data->hour, voice_data->minute);
		call = rm_call_entry_new(RM_CALL_ENTRY_TYPE_VOICE, date_time, "", voice_data->remote_number, "", voice_data->local_number, "0:01", g_strdup(voice_data->file));
		journal = rm_journal_add_call_entry(journal, call);
	}

	return journal;
}

/**
 * \brief Load voicebox and add it to journal
 * \param journal journal call list
 * \return journal list with added voicebox
 */
GList *fritzbox_load_voicebox(GList *journal)
{
	RmFtp *client;
	gchar *path;
	gint index;
	RmProfile *profile = rm_profile_get_active();
	gchar *user = rm_router_get_ftp_user(profile);
	gchar *volume_path;

	client = rm_ftp_init(rm_router_get_host(profile));
	if (!client) {
		g_warning("Could not init ftp connection. Please check that ftp is enabled");
		return journal;
	}

	if (!rm_ftp_login(client, user, rm_router_get_ftp_password(profile))) {
		g_warning("Could not login to router ftp");
		rm_object_emit_message(R_("FTP Login failed"), R_("Please check your ftp credentials"));
		rm_ftp_shutdown(client);
		return journal;
	}

	volume_path = g_settings_get_string(fritzbox_settings, "fax-volume");
	path = g_build_filename(volume_path, "FRITZ/voicebox/", NULL);
	g_free(volume_path);

	for (index = 0; index < 5; index++) {
		gchar *file = g_strdup_printf("%smeta%d", path, index);
		gchar *file_data;
		gsize file_size = 0;

		if (!rm_ftp_passive(client)) {
			g_warning("Could not switch to passive mode");
			break;
		}

		file_data = rm_ftp_get_file(client, file, &file_size);
		g_free(file);

		if (file_data && file_size) {
			voice_boxes[index].len = file_size;
			voice_boxes[index].data = g_malloc(voice_boxes[index].len);
			memcpy(voice_boxes[index].data, file_data, file_size);
			journal = fritzbox_parse_voice_data(journal, file_data, file_size);
			g_free(file_data);
		} else {
			g_free(file_data);
			break;
		}
	}
	g_free(path);

	rm_ftp_shutdown(client);

	return journal;
}

extern gboolean fritzbox_use_tr64;

/**
 * \brief Load fax file via FTP
 * \param profile profile structure
 * \param filename fax filename
 * \param len pointer to store the data length to
 * \return fax data
 */
gchar *fritzbox_load_fax(RmProfile *profile, const gchar *filename, gsize *len)
{
	gchar *ret = NULL;

	g_debug("%s(): filename %s", __FUNCTION__, filename ? filename : "NULL");
	if (fritzbox_use_tr64) {
		SoupMessage *msg;
		gchar *url;

		if (!rm_router_login(profile)) {
			return ret;
		}

		/* Create message */
		url = g_strdup_printf("https://%s:49443%s&sid=%s", rm_router_get_host(profile), filename, profile->router_info->session_id);
		msg = soup_message_new(SOUP_METHOD_GET, url);

		g_free(url);

		soup_session_send_message(rm_soup_session, msg);

		if (msg->status_code != 200) {
			g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
			rm_file_save("loadfax-error.xml", msg->response_body->data, -1);
			g_object_unref(msg);
			return NULL;
		}

		ret = g_malloc0(msg->response_body->length);
		memcpy(ret, msg->response_body->data, msg->response_body->length);
		*len = msg->response_body->length;

		g_object_unref(msg);
	} else {
		RmFtp *client;
		gchar *user = rm_router_get_ftp_user(profile);

		client = rm_ftp_init(rm_router_get_host(profile));
		rm_ftp_login(client, user, rm_router_get_ftp_password(profile));

		rm_ftp_passive(client);

		ret = rm_ftp_get_file(client, filename, len);
		rm_ftp_shutdown(client);
	}

	return ret;
}

/**
 * \brief Load voice file via FTP
 * \param profile profile structure
 * \param name voice filename
 * \param len pointer to store the data length to
 * \return voice data
 */
gchar *fritzbox_load_voice(RmProfile *profile, const gchar *filename, gsize *len)
{
	gchar *ret = NULL;

	g_debug("%s(): filename %s", __FUNCTION__, filename ? filename : "NULL");
	if (fritzbox_use_tr64) {
		return firmware_tr64_load_voice(profile, filename, len);
	} else {
		RmFtp *client;
		gchar *name = g_strconcat("/", g_settings_get_string(fritzbox_settings, "fax-volume"), "/FRITZ/voicebox/rec/", filename, NULL);
		gchar *user = rm_router_get_ftp_user(profile);

		client = rm_ftp_init(rm_router_get_host(profile));
		if (!client) {
			g_debug("Could not init ftp connection");
			return ret;
		}

		rm_ftp_login(client, user, rm_router_get_ftp_password(profile));

		rm_ftp_passive(client);

		ret = rm_ftp_get_file(client, name, len);

		rm_ftp_shutdown(client);

		g_free(name);
	}

	return ret;
}

/**
 * \brief Find phone port by dial port
 * \param dial_port dial port number
 * \param phone port on success, otherwise -1
 */
gint fritzbox_find_phone_port(gint dial_port)
{
	gint index;

	for (index = 0; index < PORT_MAX; index++) {
		if (fritzbox_phone_ports[index].number == dial_port) {
			return fritzbox_phone_ports[index].type;
		}
	}

	return -1;
}

/**
 * \brief Extract IP address from router
 * \param profile profile pointer
 * \return current IP address or NULL on error
 */
gchar *fritzbox_get_ip(RmProfile *profile)
{
	SoupMessage *msg;
	SoupURI *uri;
	gchar *ip = NULL;
	gchar *request;
	SoupMessageHeaders *headers;
	gchar *url;

	/* Create POST message */
	if (FIRMWARE_IS(6, 6)) {
		url = g_strdup_printf("http://%s/igdupnp/control/WANIPConn1", rm_router_get_host(profile));
	} else {
		url = g_strdup_printf("http://%s/upnp/control/WANIPConn1", rm_router_get_host(profile));
	}

	uri = soup_uri_new(url);
	soup_uri_set_port(uri, 49000);
	msg = soup_message_new_from_uri(SOUP_METHOD_POST, uri);
	g_free(url);

	request = g_strdup(
		"<?xml version='1.0' encoding='utf-8'?>"
		" <s:Envelope s:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/' xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'>"
		" <s:Body>"
		" <u:GetExternalIPAddress xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\" />"
		" </s:Body>"
		" </s:Envelope>\r\n");

	soup_message_set_request(msg, "text/xml; charset=\"utf-8\"", SOUP_MEMORY_STATIC, request, strlen(request));
	headers = msg->request_headers;
	soup_message_headers_append(headers, "SoapAction", "urn:schemas-upnp-org:service:WANIPConnection:1#GetExternalIPAddress");

	soup_session_send_message(rm_soup_session, msg);

	if (msg->status_code != 200) {
		g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
		g_object_unref(msg);
		return NULL;
	}

	ip = rm_utils_xml_extract_tag(msg->response_body->data, "NewExternalIPAddress");

	g_object_unref(msg);

	g_debug("Got IP data (%s)", ip);

	return ip;
}

/**
 * \brief Reconnect network
 * \param profile profile pointer
 * \return TRUE on success, otherwise FALSE
 */
gboolean fritzbox_reconnect(RmProfile *profile)
{
	SoupMessage *msg;
	SoupURI *uri;
	gchar *request;
	SoupMessageHeaders *headers;
	gchar *url;

	/* Create POST message */
	if (FIRMWARE_IS(6, 6)) {
		url = g_strdup_printf("http://%s:49000/igdupnp/control/WANIPConn1", rm_router_get_host(profile));
	} else {
		url = g_strdup_printf("http://%s:49000/upnp/control/WANIPConn1", rm_router_get_host(profile));
	}

	uri = soup_uri_new(url);
	soup_uri_set_port(uri, 49000);
	msg = soup_message_new_from_uri(SOUP_METHOD_POST, uri);
	g_free(url);

	request = g_strdup(
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
		" <s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
		" <s:Body>"
		" <u:ForceTermination xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\" />"
		" </s:Body>"
		" </s:Envelope>\r\n");

	soup_message_set_request(msg, "text/xml; charset=\"utf-8\"", SOUP_MEMORY_STATIC, request, strlen(request));
	headers = msg->request_headers;
	soup_message_headers_append(headers, "SoapAction", "urn:schemas-upnp-org:service:WANIPConnection:1#ForceTermination");

	soup_session_send_message(rm_soup_session, msg);

	if (msg->status_code != 200) {
		g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
		g_object_unref(msg);
		return FALSE;
	}

	g_object_unref(msg);

	return TRUE;
}

/**
 * \brief Delete fax file from router
 * \param profile profile pointer
 * \param filename fax filename to delete
 * \return TRUE on success, otherwise FALSE
 */
gboolean fritzbox_delete_fax(RmProfile *profile, const gchar *filename)
{
	RmFtp *client;
	gchar *user = rm_router_get_ftp_user(profile);
	gboolean ret;

	client = rm_ftp_init(rm_router_get_host(profile));
	rm_ftp_login(client, user, rm_router_get_ftp_password(profile));

	rm_ftp_passive(client);

	ret = rm_ftp_delete_file(client, filename);
	rm_ftp_shutdown(client);

	return ret;
}

/**
 * \brief Delete voice file from router
 * \param profile profile pointer
 * \param filename voice filename to delete
 * \return TRUE on success, otherwise FALSE
 */
gboolean fritzbox_delete_voice(RmProfile *profile, const gchar *filename)
{
	RmFtp *client;
	struct voice_data *voice_data;
	gpointer modified_data;
	gint nr;
	gint count;
	gint index;
	gint offset = 0;
	gchar *name;

	nr = filename[4] - '0';
	if (!voice_boxes[nr].data || voice_boxes[nr].len == 0) {
		return FALSE;
	}

	/* Modify data */
	count = voice_boxes[nr].len / sizeof(struct voice_data);
	modified_data = g_malloc((count - 1) * sizeof(struct voice_data));

	for (index = 0; index < count; index++) {
		voice_data = (struct voice_data *)(voice_boxes[nr].data + index * sizeof(struct voice_data));
		if (strncmp(voice_data->file, filename, strlen(filename)) != 0) {
			memcpy(modified_data + offset, voice_boxes[nr].data + index * sizeof(struct voice_data), sizeof(struct voice_data));
			offset += sizeof(struct voice_data);
		}
	}

	/* Write data to router */
	client = rm_ftp_init(rm_router_get_host(profile));
	rm_ftp_login(client, rm_router_get_ftp_user(profile), rm_router_get_ftp_password(profile));

	gchar *path = g_build_filename(g_settings_get_string(fritzbox_settings, "fax-volume"), "FRITZ/voicebox/", NULL);
	gchar *remote_file = g_strdup_printf("meta%d", nr);
	if (!rm_ftp_put_file(client, remote_file, path, modified_data, offset)) {
		g_free(modified_data);
		g_free(remote_file);
		g_free(path);
		rm_ftp_shutdown(client);
		return FALSE;
	}

	g_free(remote_file);
	g_free(path);

	/* Modify internal data structure */
	g_free(voice_boxes[nr].data);
	voice_boxes[nr].data = modified_data;
	voice_boxes[nr].len = offset;

	/* Delete voice file */
	name = g_build_filename(g_settings_get_string(fritzbox_settings, "fax-volume"), "FRITZ/voicebox/rec", filename, NULL);
	if (!rm_ftp_delete_file(client, name)) {
		g_free(name);
		rm_ftp_shutdown(client);
		return FALSE;
	}

	rm_ftp_shutdown(client);

	g_free(name);

	return TRUE;
}

