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

#include <libsoup/soup.h>

#include <string.h>
#include <rm/rmstring.h>
#include <rm/rmnetwork.h>
#include <rm/rmobjectemit.h>
#include <rm/rmprofile.h>
#include <rm/rmrouter.h>
#include <rm/rmmain.h>
#include <rm/rmlog.h>
#include <rm/rmutils.h>

#define FIRMWARE_TR64_DEBUG 1

/**
 * SECTION:rmnetwork
 * @Title: RmNetwork
 * @Short_description: Network - Handle network access
 *
 * A network wrapper with authentication support
 */

#define SOUP_MSG_START  "<?xml version='1.0' encoding='utf-8'?>" \
	"<s:Envelope s:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/' xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'>"

#define SOUP_MSG_END "</s:Envelope>\r\n"
#define SOUP_MSG_HEADER_START "<s:Header>"
#define SOUP_MSG_HEADER_END "</s:Header>"
#define SOUP_MSG_BODY_START "<s:Body>"
#define SOUP_MSG_BODY_END "</s:Body>"

/**
 * rm_soup_session:
 *
 * Global soup session.
 */
SoupSession *rm_soup_session = NULL;

static gchar *auth_header = NULL;
static gint tr64_security_port = 0;

/**
 * md5_simple:
 * @input: input string
 *
 * Compute md5 sum of input string
 *
 * Returns: md5 in hex or NULL on error
 */
static inline gchar *md5_simple(gchar *input)
{
	GError *error = NULL;
	gchar *ret = NULL;

	if (error == NULL) {
		ret = g_compute_checksum_for_string(G_CHECKSUM_MD5, (gchar*)input, -1);
	} else {
		g_debug("Error converting utf8 to utf16: '%s'", error->message);
		g_error_free(error);
	}

	return ret;
}

/**
 * rm_network_tr64_create_response:
 * @nonce: a nonce
 * @realm: router realm
 * @user: router user
 * @password: router password
 *
 * Create authentication token
 *
 * Returns: authentication token
 */
static gchar *rm_network_tr64_create_response(gchar *nonce, gchar *realm, gchar *user, gchar *password)
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
 * rm_network_tr64_request:
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
SoupMessage *rm_network_tr64_request(RmProfile *profile, gboolean auth, gchar *control, gchar *action, gchar *service, ...)
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
		port = tr64_security_port;

		if (!auth_header) {
			g_string_append_printf(request, SOUP_MSG_HEADER_START "<h:InitChallenge xmlns:h=\"http://soap-authentication.org/digest/2001/10/\" s:mustUnderstand=\"1\">\
			        <UserID>%s</UserID></h:InitChallenge>" SOUP_MSG_HEADER_END, login_user);
		} else {
			g_string_append(request, auth_header);
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
	rm_log_save_data("tr64-request-ok-1.xml", msg->response_body->data, msg->response_body->length);

	status = rm_utils_xml_extract_tag(msg->response_body->data, "Status");
	if (status && !strcmp(status, "Unauthenticated")) {
#ifdef FIRMWARE_TR64_DEBUG
		g_debug("%s(): Login required", __FUNCTION__);
#endif
		gchar *nonce = rm_utils_xml_extract_tag(msg->response_body->data, "Nonce");
		gchar *realm = rm_utils_xml_extract_tag(msg->response_body->data, "Realm");
		gchar *response;
		gchar *new_auth_header = NULL;

		response = rm_network_tr64_create_response(nonce, realm, login_user, rm_router_get_login_password(profile));

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

		rm_log_save_data("tr64-request-ok-2.xml", msg->response_body->data, msg->response_body->length);
	}

	return msg;
}

/**
 * rm_network_tr64_get_security_port:
 * @profile: a #RmProfile
 *
 * Retrieve security port
 *
 * Returns: security port number if available, otherwise 0
 */
gboolean rm_network_tr64_available(RmProfile *profile)
{
	g_autoptr(SoupMessage) msg = NULL;
	g_autofree gchar *port = NULL;

	msg = rm_network_tr64_request(profile, FALSE, "deviceinfo", "GetSecurityPort", "urn:dslforum-org:service:DeviceInfo:1", NULL);
	if (msg == NULL) {
		return 0;
	}

	port = rm_utils_xml_extract_tag(msg->response_body->data, "NewSecurityPort");
	tr64_security_port = port ? atoi(port) : 0;

	return tr64_security_port != 0;
}

gint rm_network_tr64_get_port(void)
{
	return tr64_security_port;
}

/**
 * rm_network_free_auth_data:
 * @auth_data: a #RmAuthData
 *
 * Releases and frees authentication data.
 */
static void rm_network_free_auth_data(RmAuthData *auth_data)
{
	g_object_unref(auth_data->msg);

	g_free(auth_data->username);
	g_free(auth_data->password);

	g_slice_free(RmAuthData, auth_data);
}

/**
 * rm_network_save_password_cb:
 * @msg: a #SoupMessage
 * @auth_data: a #RmAuthData
 *
 * Saves authentication credentials if network transfer was successful.
 */
static void rm_network_save_password_cb(SoupMessage* msg, RmAuthData *auth_data)
{
	if (msg->status_code != 401 && msg->status_code < 500) {
		RmProfile *profile = rm_profile_get_active();

		g_settings_set_string(profile->settings, "auth-user", auth_data->username);
		g_settings_set_string(profile->settings, "auth-password", auth_data->password);

		g_debug("%s(): Storing data for later processing", __FUNCTION__);
	}

	g_signal_handlers_disconnect_by_func(msg, rm_network_save_password_cb, auth_data);

	rm_network_free_auth_data(auth_data);
}

/**
 * rm_network_authenticate:
 * @auth_set: indicated whether authtentification data has been set
 * @auth_data: a #RmAuthData
 *
 * Authenticate within network with @auth_data
 */
void rm_network_authenticate(gboolean auth_set, RmAuthData *auth_data)
{
	g_debug("%s(): calling authenticate", __FUNCTION__);

	if (auth_set) {
		soup_auth_authenticate(auth_data->auth, auth_data->username, auth_data->password);
		g_signal_connect(auth_data->msg, "got-headers", G_CALLBACK(rm_network_save_password_cb), auth_data);
	}

	soup_session_unpause_message(auth_data->session, auth_data->msg);
	if (!auth_set) {
		rm_network_free_auth_data(auth_data);
	}
}

/**
 * rm_network_authenticate_cb:
 * @sesion: a #SoupSession
 * @msg: a #SoupMessage
 * @auth: a #SoupAuth
 * @retrying: flag indicating if method is called again
 * @user_data: user data
 *
 * A network authentication is required. Retrieve information from settings or ask user.
 */
static void rm_network_authenticate_cb(SoupSession *session, SoupMessage *msg, SoupAuth *auth, gboolean retrying, gpointer user_data)
{
	RmAuthData *auth_data;
	RmProfile *profile = rm_profile_get_active();
	const gchar *user;
	const gchar *password;

	g_debug("%s(): retrying: %d, status code: %d == %d", __FUNCTION__, retrying, msg->status_code, SOUP_STATUS_UNAUTHORIZED);
	if (msg->status_code != SOUP_STATUS_UNAUTHORIZED) {
		return;
	}

	g_debug("%s(): called with profile %p", __FUNCTION__, profile);
	if (!profile) {
		return;
	}

	soup_session_pause_message(session, msg);
	/* We need to make sure the message sticks around when pausing it */
	g_object_ref(msg);

	user = g_settings_get_string(profile->settings, "auth-user");
	password = g_settings_get_string(profile->settings, "auth-password");

	if (RM_EMPTY_STRING(user) && RM_EMPTY_STRING(password)) {
		user = rm_router_get_login_user(profile);
		g_settings_set_string(profile->settings, "auth-user", user);

		password = rm_router_get_login_password(profile);
		g_settings_set_string(profile->settings, "auth-password", password);
	}

	if (!retrying && !RM_EMPTY_STRING(user) && !RM_EMPTY_STRING(password)) {
		g_debug("%s(): Already configured...", __FUNCTION__);
		soup_auth_authenticate(auth, user, password);

		soup_session_unpause_message(session, msg);
	} else {
		auth_data = g_slice_new0(RmAuthData);

		auth_data->msg = msg;
		auth_data->auth = auth;
		auth_data->session = session;
		auth_data->retry = retrying;
		auth_data->username = g_strdup(user);
		auth_data->password = g_strdup(password);

		rm_object_emit_authenticate(auth_data);
	}
}

/**
 * rm_network_init:
 *
 * Initialize network functions.
 *
 * Returns: TRUE on success, otherwise FALSE
 */
gboolean rm_network_init(void)
{
	SoupCache *cache;

	/* NULL for directory is not sufficient on Windows platform, therefore set user cache dir */
	cache = soup_cache_new(rm_get_user_cache_dir(), SOUP_CACHE_SINGLE_USER);
	rm_soup_session = soup_session_new_with_options(SOUP_SESSION_TIMEOUT, 5, SOUP_SESSION_USE_THREAD_CONTEXT, TRUE, SOUP_SESSION_ADD_FEATURE, cache, SOUP_SESSION_SSL_STRICT, FALSE, NULL);
	soup_cache_load(cache);

	g_signal_connect(rm_soup_session, "authenticate", G_CALLBACK(rm_network_authenticate_cb), rm_soup_session);

	return rm_soup_session != NULL;
}

/**
 * rm_network_shutdown:
 *
 * Shutdown network infrastructure.
 */
void rm_network_shutdown(void)
{
	g_clear_object(&rm_soup_session);
}
