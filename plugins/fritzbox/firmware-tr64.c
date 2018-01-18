/*
 * The rm project
 * Copyright (c) 2012-2018 Jan-Michael Brummer
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

#include <libgupnp/gupnp.h>
#include <libgupnp/gupnp-device-info.h>

#include <rm/rm.h>

#include "fritzbox.h"
#include "firmware-common.h"
#include "firmware-query.h"

#define SOUP_MSG_START  "<?xml version='1.0' encoding='utf-8'?>" \
	"<s:Envelope s:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/' xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'>"

#define SOUP_MSG_END "</s:Envelope>\r\n"
#define SOUP_MSG_HEADER_START "<s:Header>"
#define SOUP_MSG_HEADER_END "</s:Header>"
#define SOUP_MSG_BODY_START "<s:Body>"
#define SOUP_MSG_BODY_END "</s:Body>"

//#define FIRMWARE_TR64_DEBUG 1

static gint firmware_tr64_security_port = 0;
static gchar *auth_header = NULL;

/**
 * firmware_tr64_create_response:
 * @nonce: a nonce
 * @realm: router realm
 * @user: router user
 * @password: router password
 *
 * Create authentication token
 *
 * Returns: authentication token
 */
gchar *firmware_tr64_create_response(gchar *nonce, gchar *realm, gchar *user, gchar *password)
{
	/** secret = MD5( concat(uid, ":", realm, ":", pwd) ) */
	g_autofree gchar *secret = g_strconcat(user, ":", realm, ":", password, NULL);
	g_autofree gchar *secret_md5 = md5_simple(secret);
	/** response = MD5( concat(secret, ":", sn) ) */
	g_autofree gchar *response = g_strconcat(secret_md5, ":", nonce, NULL);
	gchar *response_md5 = md5_simple(response);

	return response_md5;
}

/**
 * firmware_tr64_request:
 * @profile: a #RmProfile
 * @auth: authentication required flag
 * @control: upnp control
 * @action: soap action
 * @service: soap service
 *
 * Send a tr64 soap request
 *
 * Returns: #SoupMessage as a result of tr64 send request
 */
static SoupMessage *firmware_tr64_request(RmProfile *profile, gboolean auth, gchar *control, gchar *action, gchar *service, ...)
{
	SoupMessage *msg;
	SoupMessageHeaders *headers;
	GString *request = g_string_new(SOUP_MSG_START);
	SoupURI *uri;
	gchar *status;
	gint port;
	g_autofree gchar *login_user = rm_router_get_login_user(profile);
	g_autofree gchar *host = rm_router_get_host(profile);
	g_autofree gchar *url = NULL;

	if (RM_EMPTY_STRING(login_user)) {
		login_user = g_strdup("admin");
	}

	if (!auth) {
		url = g_strdup_printf("http://%s/upnp/control/%s", host, control);
		port = 49000;
	} else {
		url = g_strdup_printf("https://%s/upnp/control/%s", host, control);
		port = firmware_tr64_security_port;

		if (!auth_header) {
			g_string_append_printf(request, SOUP_MSG_HEADER_START "<h:InitChallenge xmlns:h=\"http://soap-authentication.org/digest/2001/10/\" s:mustUnderstand=\"1\">\
			        <UserID>%s</UserID></h:InitChallenge>" SOUP_MSG_HEADER_END, login_user);
		} else {
			g_string_append_printf(request, auth_header);
		}
	}

	uri = soup_uri_new(url);
	soup_uri_set_port(uri, port);
	msg = soup_message_new_from_uri(SOUP_METHOD_POST, uri);

	g_string_append_printf(request, SOUP_MSG_BODY_START "<u:%s xmlns:u='%s'>", action, service);

	va_list arg;
	gchar *key;

	va_start(arg, service);
	while ((key = va_arg(arg, char *)) != NULL) {
		gchar *val = va_arg(arg, char *);
		g_string_append_printf(request, "<%s>%s</%s>", key, val, key);
	}
	va_end(arg);
	g_string_append_printf(request, "</u:%s>" SOUP_MSG_BODY_END SOUP_MSG_END, action);

#ifdef FIRMWARE_TR64_DEBUG
	g_debug("%s(): SoupRequest: %s", __FUNCTION__, request->str);
#endif

	soup_message_set_request(msg, "text/xml; charset=\"utf-8\"", SOUP_MEMORY_STATIC, request->str, request->len);
	headers = msg->request_headers;
	gchar *header = g_strdup_printf("%s#%s", service, action);
	soup_message_headers_append(headers, "SoapAction", header);

	soup_session_send_message(rm_soup_session, msg);
	g_string_free(request, TRUE);

	if (msg->status_code != SOUP_STATUS_OK) {
		g_debug("%s(): Received status code: %d (%s)", __FUNCTION__, msg->status_code, soup_status_get_phrase(msg->status_code));
		rm_log_save_data("tr64-request-error1.xml", msg->response_body->data, -1);
		g_object_unref(msg);

		return NULL;
	}
#ifdef FIRMWARE_TR64_DEBUG
	rm_log_save_data("tr64-request-ok-1.xml", msg->response_body->data, msg->response_body->length);
#endif

	status = xml_extract_tag(msg->response_body->data, "Status");
	if (status && !strcmp(status, "Unauthenticated")) {
#ifdef FIRMWARE_TR64_DEBUG
		g_debug("%s(): Login required", __FUNCTION__);
#endif
		gchar *nonce = xml_extract_tag(msg->response_body->data, "Nonce");
		gchar *realm = xml_extract_tag(msg->response_body->data, "Realm");
		gchar *response;
		gchar *new_auth_header = NULL;

		response = firmware_tr64_create_response(nonce, realm, login_user, rm_router_get_login_password(profile));

		request = g_string_new(SOUP_MSG_START);

		new_auth_header = g_strdup_printf(
				       SOUP_MSG_HEADER_START
				       "<h:ClientAuth xmlns:h='http://soap-authentication.org/digest/2001/10/' s:mustUnderstand='1'>"
				       "<Nonce>%s</Nonce>"
				       "<Auth>%s</Auth>"
				       "<UserID>%s</UserID>"
				       "<Realm>%s</Realm>"
				       "</h:ClientAuth>"
				       SOUP_MSG_HEADER_END,
				       nonce, response, login_user, realm);

		g_string_append(request, new_auth_header);
		g_string_append_printf(request, SOUP_MSG_BODY_START "<u:%s xmlns:u='%s'>", action, service);

		va_list arg;
		gchar *key;

		va_start(arg, service);
		while ((key = va_arg(arg, char *)) != NULL) {
			gchar *val = va_arg(arg, char *);
			g_string_append_printf(request, "<%s>%s</%s>", key, val, key);
		}
		va_end(arg);
		g_string_append_printf(request, "</u:%s>" SOUP_MSG_BODY_END SOUP_MSG_END, action);

		g_object_unref(msg);
		msg = soup_message_new_from_uri(SOUP_METHOD_POST, uri);

#ifdef FIRMWARE_TR64_DEBUG
		g_debug("%s(): SoupRequest (auth): %s", __FUNCTION__, request->str);
#endif

		soup_message_set_request(msg, "text/xml; charset=\"utf-8\"", SOUP_MEMORY_STATIC, request->str, request->len);
		headers = msg->request_headers;
		gchar *header = g_strdup_printf("%s#%s", service, action);
		soup_message_headers_append(headers, "SoapAction", header);

		soup_session_send_message(rm_soup_session, msg);
		g_string_free(request, FALSE);

		if (msg->status_code != SOUP_STATUS_OK) {
			g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
			rm_log_save_data("tr64-request-error2.xml", msg->response_body->data, -1);
			g_object_unref(msg)
;
			auth_header = NULL;

			return NULL;
		}

		auth_header = new_auth_header;

#ifdef FIRMWARE_TR64_DEBUG
		rm_log_save_data("tr64-request-ok-2.xml", msg->response_body->data, msg->response_body->length);
#endif
	}

	return msg;
}

/**
 * firmware_tr64_get_security_port:
 * @profile: a #RmProfile
 *
 * Retrieve security port
 *
 * Returns: security port number if available, otherwise 0
 */
static gint firmware_tr64_get_security_port(RmProfile *profile)
{
	g_autoptr(SoupMessage) msg = NULL;
	g_autofree gchar *port = NULL;

	msg = firmware_tr64_request(profile, FALSE, "deviceinfo", "GetSecurityPort", "urn:dslforum-org:service:DeviceInfo:1", NULL);
	if (msg == NULL) {
		return 0;
	}

	port = xml_extract_tag(msg->response_body->data, "NewSecurityPort");

	return port ? atoi(port) : 0;
}

/**
 * firmware_tr64_add_call:
 * @list: journal list
 * @profile: a #RmProfile
 * @call: xml master node of call element
 *
 * Add @call to journal @list
 *
 * Returns: Updated journal @list
 */
static GSList *firmware_tr64_add_call(GSList *list, RmProfile *profile, RmXmlNode *call)
{
	gchar *type;
	gchar *called;
	gchar *caller;
	gchar *name;
	gchar *port;
	gchar *date_time;
	gchar *duration;
	gchar *path;
	gchar *remote_name;
	gchar *remote_number;
	gchar *local_name;
	gchar *local_number;
	RmXmlNode *tmp;
	RmCallEntry *call_entry;
	RmCallEntryTypes call_type;

	tmp = rm_xmlnode_get_child(call, "Type");
	type = rm_xmlnode_get_data(tmp);
	tmp = rm_xmlnode_get_child(call, "Name");
	name = rm_xmlnode_get_data(tmp);
	tmp = rm_xmlnode_get_child(call, "Duration");
	duration = rm_xmlnode_get_data(tmp);
	tmp = rm_xmlnode_get_child(call, "Date");
	date_time = rm_xmlnode_get_data(tmp);
	tmp = rm_xmlnode_get_child(call, "Device");
	local_name = rm_xmlnode_get_data(tmp);
	tmp = rm_xmlnode_get_child(call, "Path");
	path = rm_xmlnode_get_data(tmp);
	tmp = rm_xmlnode_get_child(call, "Port");
	port = rm_xmlnode_get_data(tmp);

	remote_name = name;

	if (atoi(type) == 3) {
		tmp = rm_xmlnode_get_child(call, "CallerNumber");
		caller = rm_xmlnode_get_data(tmp);
		tmp = rm_xmlnode_get_child(call, "Called");
		called = rm_xmlnode_get_data(tmp);

		remote_number = called;
		local_number = caller;
	} else {
		tmp = rm_xmlnode_get_child(call, "CalledNumber");
		called = rm_xmlnode_get_data(tmp);
		tmp = rm_xmlnode_get_child(call, "Caller");
		caller = rm_xmlnode_get_data(tmp);

		remote_number = caller;
		local_number = called;
	}

	call_type = atoi(type);

	if (call_type == 10) {
		call_type = RM_CALL_ENTRY_TYPE_BLOCKED;
	}

#ifdef FIRMWARE_TR64_DEBUG
	if (!RM_EMPTY_STRING(path)) {
		g_debug("%s(): path %s, port %s", __FUNCTION__, path, port);
	}
#endif

	if (port && path) {
		gint port_nr = atoi(port);

		if (port_nr == 6 || (port_nr >= 40 && port_nr < 50)) {
			call_type = RM_CALL_ENTRY_TYPE_VOICE;
		} else if (port_nr == 5) {
			call_type = RM_CALL_ENTRY_TYPE_FAX;
#ifdef FIRMWARE_TR64_DEBUG
			g_debug("%s(): path: %s", __FUNCTION__, path);
#endif
		}
	}

	call_entry = rm_call_entry_new(call_type, date_time, remote_name, remote_number, local_name, local_number, duration, g_strdup(path));
	list = rm_journal_add_call_entry(list, call_entry);

	return list;
}

/**
 * firmware_tr64_journal_cb:
 * @session: a #SoupSession
 * @msg: a #SoupMessage
 * @user_data: a #RmProfile
 *
 * Get journal callback
 */
void firmware_tr64_journal_cb(SoupSession *session, SoupMessage *msg, gpointer user_data)
{
	GSList *journal = NULL;
	RmProfile *profile = user_data;
	RmXmlNode *child;

	if (msg->status_code != SOUP_STATUS_OK) {
		g_debug("%s(): Got invalid data, return code: %d (%s)", __FUNCTION__, msg->status_code, soup_status_get_phrase(msg->status_code));
		g_object_unref(msg);
		return;
	}

#ifdef FIRMWARE_TR64_DEBUG
	rm_log_save_data("tr64-callist.xml", msg->response_body->data, msg->response_body->length);
#endif

	RmXmlNode *node = rm_xmlnode_from_str(msg->response_body->data, msg->response_body->length);
	if (node == NULL) {
		g_object_unref(msg);
		return;
	}

	for (child = rm_xmlnode_get_child(node, "Call"); child != NULL; child = rm_xmlnode_get_next_twin(child)) {
		journal = firmware_tr64_add_call(journal, profile, child);
	}

#ifdef FIRMWARE_TR64_DEBUG
	g_debug("%s(): process journal (%d)", __FUNCTION__, g_slist_length(journal));
#endif

	/* Load fax reports */
	journal = rm_router_load_fax_reports(profile, journal);

	/* Load voice records */
	journal = rm_router_load_voice_records(profile, journal);

	/* Process journal list */
	rm_router_process_journal(journal);
}

/**
 * firmware_tr64_load_journal:
 * @profile: a #RmProfile
 *
 * Load journal using x_contact interface
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean firmware_tr64_load_journal(RmProfile *profile)
{
	g_autoptr(SoupMessage) url_msg = NULL;
	SoupMessage *msg = NULL;
	g_autofree gchar *url = NULL;

	url_msg = firmware_tr64_request(profile, TRUE, "x_contact", "GetCallList", "urn:dslforum-org:service:X_AVM-DE_OnTel:1", NULL);
	if (url_msg == NULL) {
		return FALSE;
	}

	url = xml_extract_tag(url_msg->response_body->data, "NewCallListURL");
	if (RM_EMPTY_STRING(url)) {
		return FALSE;
	}

#ifdef FIRMWARE_TR64_DEBUG
	rm_log_save_data("tr64-getcalllist.xml", url_msg->response_body->data, url_msg->response_body->length);
#endif

	msg = soup_message_new(SOUP_METHOD_GET, url);

	/* Queue message to session */
	soup_session_queue_message(rm_soup_session, msg, firmware_tr64_journal_cb, profile);

	return TRUE;
}

/**
 * firmware_tr64_load_voice:
 * @profile: a #RmProfile
 * @filename: voice filename
 * @len: length of voice data
 *
 * Load voice file using TR64
 *
 * Returns: voice data or %NULL on error
 */
gchar *firmware_tr64_load_voice(RmProfile *profile, const gchar *filename, gsize *len)
{
	g_autoptr(SoupMessage) msg = NULL;
	g_autofree gchar *url = NULL;
	g_autofree gchar *host = rm_router_get_host(profile);

	if (!rm_router_login(profile)) {
		return NULL;
	}

	/* Create message */
	url = g_strdup_printf("https://%s:%d%s&sid=%s", host, firmware_tr64_security_port, filename, profile->router_info->session_id);
	msg = soup_message_new(SOUP_METHOD_GET, url);

	soup_session_send_message(rm_soup_session, msg);

	if (msg->status_code != SOUP_STATUS_OK) {
		g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
		rm_log_save_data("tr64-loadvoice-error.xml", msg->response_body->data, -1);
		return NULL;
	}

	*len = msg->response_body->length;

	return g_memdup(msg->response_body->data, *len);
}

/**
 * firmware_tr64_dial_number:
 * @profile: a #RmProfile
 * @port: dial port
 * @number: target number
 *
 * Dial @number
 *
 * Returns: %TRUE whether number could be dialed, %FALSE on error
 */
gboolean firmware_tr64_dial_number(RmProfile *profile, gint port, const gchar *number)
{
	g_autoptr(SoupMessage) msg = NULL;
	gint idx = -1;
	gint i;

	for (i = 0; i < PORT_MAX - 2; i++) {
		if (fritzbox_phone_ports[i].type == port) {
			idx = i;
			break;
		}
	}

	if (idx == -1) {
		return FALSE;
	}

	msg = firmware_tr64_request(profile, TRUE, "x_voip", "X_AVM-DE_DialSetConfig", "urn:dslforum-org:service:X_VoIP:1", "NewX_AVM-DE_PhoneName", fritzbox_phone_ports[idx].setting_name, NULL);
	if (!msg) {
		return FALSE;
	}

	if (msg->status_code != SOUP_STATUS_OK) {
		g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
		rm_log_save_data("tr64-dialsetconfig-error.xml", msg->response_body->data, -1);
		return FALSE;
	}

	/* Now dial number */
	msg = firmware_tr64_request(profile, TRUE, "x_voip", "X_AVM-DE_DialNumber", "urn:dslforum-org:service:X_VoIP:1", "NewX_AVM-DE_PhoneNumber", number, NULL);
	if (!msg) {
		return FALSE;
	}

	if (msg->status_code != SOUP_STATUS_OK) {
		g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
		rm_log_save_data("tr64-dialnumber-error.xml", msg->response_body->data, -1);
		return FALSE;
	}

	return TRUE;
}

/**
 * firmware_tr64_get_settings:
 * @profile: a #RmProfile
 *
 * Get settings of router
 *
 * Returns: %TRUE on success
 */
gboolean firmware_tr64_get_settings(RmProfile *profile)
{
	RmXmlNode *node;
	RmXmlNode *child;
	g_autoptr(SoupMessage) msg = NULL;
	GRegex *lt;
	GRegex *gt;
	gchar *new_number_list;
	gchar *lt_out;
	gchar *gt_out;
	gchar *fax_msn;
	gchar **numbers = NULL;
	gchar *areacode;
	gchar *okz_prefix;
	gchar *countrycode;
	gchar *lkz_prefix;
	gsize i;
	gsize len;

	g_test_timer_start();
	msg = firmware_tr64_request(profile, TRUE, "x_voip", "X_AVM-DE_GetNumbers", "urn:dslforum-org:service:X_VoIP:1", NULL);
	if (!msg) {
		return FALSE;
	}

	if (msg->status_code != SOUP_STATUS_OK) {
		g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
		return FALSE;
	}
#ifdef FIRMWARE_TR64_DEBUG
	rm_log_save_data("tr64-getnumbers.xml", msg->response_body->data, msg->response_body->length);
#endif

	/* Extract numbers */
	new_number_list = xml_extract_tag(msg->response_body->data, "NewNumberList");
	lt = g_regex_new("&lt;", G_REGEX_DOTALL | G_REGEX_OPTIMIZE, 0, NULL);
	lt_out = g_regex_replace_literal(lt, new_number_list, -1, 0, "<", 0, NULL);
	gt = g_regex_new("&gt;", G_REGEX_DOTALL | G_REGEX_OPTIMIZE, 0, NULL);
	gt_out = g_regex_replace_literal(gt, lt_out, -1, 0, ">", 0, NULL);

	node = rm_xmlnode_from_str(gt_out, msg->response_body->length);
	if (node == NULL) {
		g_debug("%s(): No node....\n", __FUNCTION__);
		return FALSE;
	}

	for (child = rm_xmlnode_get_child(node, "Item"); child != NULL; child = rm_xmlnode_get_next_twin(child)) {
		RmXmlNode *tmp;
		g_autofree gchar *type;
		g_autofree gchar *index;
		g_autofree gchar *name;
		gchar *number;

		tmp = rm_xmlnode_get_child(child, "Number");
		number = rm_xmlnode_get_data(tmp);
		tmp = rm_xmlnode_get_child(child, "Type");
		type = rm_xmlnode_get_data(tmp);
		tmp = rm_xmlnode_get_child(child, "Index");
		index = rm_xmlnode_get_data(tmp);
		tmp = rm_xmlnode_get_child(child, "Name");
		name = rm_xmlnode_get_data(tmp);

		g_debug("%s(): %s, %s, %s, %s", __FUNCTION__, number, index, type, name);

		numbers = rm_strv_add(numbers, number);
	}
	g_settings_set_strv(profile->settings, "numbers", (const gchar*const*)numbers);

	/* Extract area code */
	msg = firmware_tr64_request(profile, TRUE, "x_voip", "GetVoIPCommonAreaCode", "urn:dslforum-org:service:X_VoIP:1", NULL);
	if (msg == NULL) {
		return FALSE;
	}

	if (msg->status_code != SOUP_STATUS_OK) {
		g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
		return FALSE;
	}

	areacode = xml_extract_tag(msg->response_body->data, "NewVoIPAreaCode");
	g_debug("%s(): Area code %s", __FUNCTION__, areacode);
	g_settings_set_string(profile->settings, "area-code", areacode + 1);

	okz_prefix = g_strdup_printf("%1.1s", areacode);
	g_settings_set_string(profile->settings, "national-access-code", okz_prefix);
	g_debug("%s(): OKZ prefix %s", __FUNCTION__, okz_prefix);

	/* Extract country code */
	msg = firmware_tr64_request(profile, TRUE, "x_voip", "GetVoIPCommonCountryCode", "urn:dslforum-org:service:X_VoIP:1", NULL);
	if (msg == NULL) {
		return FALSE;
	}

	if (msg->status_code != SOUP_STATUS_OK) {
		g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
		return FALSE;
	}

	countrycode = xml_extract_tag(msg->response_body->data, "NewVoIPCountryCode");
	g_debug("%s(): Country code %s", __FUNCTION__, countrycode);
	g_settings_set_string(profile->settings, "country-code", countrycode + 2);
	lkz_prefix = g_strdup_printf("%2.2s", countrycode);
	g_settings_set_string(profile->settings, "international-access-code", lkz_prefix);
	g_debug("%s(): LKZ prefix %s", __FUNCTION__, lkz_prefix);

	/* Set fax information */
	g_settings_set_string(profile->settings, "fax-header", "Roger Router");

	len = g_strv_length(numbers);
	g_settings_set_string(fritzbox_settings, "fax-number", "");
	g_settings_set_string(profile->settings, "fax-ident", "");

	if (len) {
		fax_msn = len > 1 ? numbers[1] : numbers[0];

		g_settings_set_string(profile->settings, "fax-number", fax_msn);

		gchar *formated_number = rm_number_format(profile, fax_msn, RM_NUMBER_FORMAT_INTERNATIONAL_PLUS);
		g_settings_set_string(profile->settings, "fax-ident", formated_number);
		g_free(formated_number);
	}

	/* Extract phone names for dialer */
	for (i = 1; i < PORT_MAX; i++) {
		gchar *phone;
		gchar *num = g_strdup_printf("%ld", i);


		msg = firmware_tr64_request(profile, TRUE, "x_voip", "X_AVM-DE_GetPhonePort", "urn:dslforum-org:service:X_VoIP:1", "NewIndex", num, NULL);
		if (msg == NULL) {
			g_settings_set_string(fritzbox_settings, fritzbox_phone_ports[i - 1].setting_name, "");
			break;
		}

		if (msg->status_code != SOUP_STATUS_OK) {
			g_debug("%s(): Received status code: %d", __FUNCTION__, msg->status_code);
			g_settings_set_string(fritzbox_settings, fritzbox_phone_ports[i - 1].setting_name, "");
			break;
		}

		phone = xml_extract_tag(msg->response_body->data, "NewX_AVM-DE_PhoneName");
		g_debug("%s(): Phone '%s' to '%s'", __FUNCTION__, phone, fritzbox_phone_ports[i - 1].setting_name);
		g_settings_set_string(fritzbox_settings, fritzbox_phone_ports[i - 1].setting_name, phone);
	}

	g_debug("%s(): Execution time: %f", __FUNCTION__, g_test_timer_elapsed());

	/* START: Set defaults for values which aren't used with TR-064 implementation */
	g_settings_set_string(fritzbox_settings, "fax-volume", "");
	g_settings_set_uint(fritzbox_settings, "port", 0);
	g_settings_set_uint(fritzbox_settings, "tam-stick", 0);
	/* END: Set defaults for values which aren't used with TR-064 implementation */

	return TRUE;
}

/**
 * firmware_tr64_is_available:
 * @profile: a #RmProfile
 *
 * Check if TR64 is available
 *
 * Returns: %TRUE if TR64 is available
 */
gboolean firmware_tr64_is_available(RmProfile *profile)
{
	firmware_tr64_security_port = firmware_tr64_get_security_port(profile);

#ifdef FIRMWARE_TR64_DEBUG
	g_debug("%s(): Security port %d", __FUNCTION__, firmware_tr64_security_port);
#endif

	return firmware_tr64_security_port != 0;
}
