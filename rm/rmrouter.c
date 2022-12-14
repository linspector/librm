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

#include <rm/rmrouter.h>
#include <rm/rmobjectemit.h>
#include <rm/rmcontact.h>
#include <rm/rmstring.h>
#include <rm/rmfile.h>
#include <rm/rmcsv.h>
#include <rm/rmpassword.h>
#include <rm/rmmain.h>
#include <rm/rmcallentry.h>
#include <rm/rmnumber.h>
#include <rm/rmjournal.h>

/**
 * SECTION:rmrouter
 * @title: RmRouter
 * @short_description: Router related functions (login, get settings, ...)
 * @stability: Stable
 *
 * Router related function to interact in an easy way. As it uses router plugins it can
 * be used with different router vendors.
 */

/** Active router structure */
static RmRouter *active_router = NULL;
/** Global router plugin list */
static GList *rm_router_list = NULL;
/** Router login blocked shield */
static gboolean rm_router_login_blocked = FALSE;

/**
 * rm_router_free_phone_info:
 * @data: pointer to phone info structure
 *
 * Free one phone info list entry.
 */
static void rm_router_free_phone_info(gpointer data)
{
	RmPhoneInfo *phone = data;

	g_free(phone->name);
}

/**
 * rm_router_free_phone_list:
 * @phone_list: phone list
 *
 * Free full phone list.
 */
void rm_router_free_phone_list(GList *phone_list)
{
	g_list_free_full(phone_list, rm_router_free_phone_info);
}

/**
 * rm_router_get_numbers:
 * @profile: a #RmProfile
 *
 * Get array of phone numbers.
 *
 * Returns: phone number array
 */
gchar **rm_router_get_numbers(RmProfile *profile)
{
	return g_settings_get_strv(profile->settings, "numbers");
}

/**
 * rm_router_present:
 * @router_info: a #RmRouterInfo
 *
 * Check if router is present.
 *
 * Returns: present state
 */
gboolean rm_router_present(RmRouterInfo *router_info)
{
	GList *list;

	if (!rm_router_list) {
		g_debug("%s(): No router plugin available", __FUNCTION__);
		return FALSE;
	}

	for (list = rm_router_list; list != NULL; list = list->next) {
		RmRouter *router = list->data;

		if (router->present(router_info)) {
			active_router = router;
			g_debug("%s(): Got active router", __FUNCTION__);
			return TRUE;
		}
	}

	g_debug("%s(): No router found", __FUNCTION__);
	return FALSE;
}

/**
 * rm_router_set_active:
 * @profile: a #RmProfile
 *
 * Set profile as active.
 */
void rm_router_set_active(RmProfile *profile)
{
	if (active_router) {
		rm_router_present(profile->router_info);
		active_router->set_active(profile);
	}
}

/**
 * rm_router_login:
 * @profile: a #RmProfile
 *
 * Login to router.
 *
 * Returns: login state
 */
gboolean rm_router_login(RmProfile *profile)
{
	gboolean result;

	if (!active_router) {
		return FALSE;
	}

	if (rm_router_login_blocked) {
		g_debug("%s(): called, but blocked", __FUNCTION__);
		return FALSE;
	}

	result = active_router->login(profile);
	if (!result) {
		g_warning("%s", R_("Login data are wrong or permissions are missing.\nPlease check your login data."));
		rm_object_emit_message(R_("Login failed"), R_("Login data are wrong or permissions are missing.\nPlease check your login data."));
		rm_router_login_blocked = TRUE;
	}

	return result;
}

/**
 * rm_router_release_lock:
 *
 * Release router login blocked
 */
void rm_router_release_lock(void)
{
	rm_router_login_blocked = FALSE;
	g_debug("%s()", __FUNCTION__);
}

/**
 * rm_router_is_locked:
 *
 * Check whether router login is blocked
 *
 * Returns: locked state
 */
gboolean rm_router_is_locked(void)
{
	g_debug("%s(): locked? %d", __FUNCTION__, rm_router_login_blocked);
	return rm_router_login_blocked;
}

/**
 * rm_router_logout:
 * @profile: a #RmProfile
 *
 * Router logout.
 *
 * Returns: logout state
 */
gboolean rm_router_logout(RmProfile *profile)
{
	return active_router ? active_router->logout(profile, FALSE) : FALSE;
}

/**
 * rm_router_get_host:
 * @profile: a #RmProfile
 *
 * Get router host
 *
 * Returns: newly allocated string with router host or "" if no profile is active
 */
gchar *rm_router_get_host(RmProfile *profile)
{
	return profile ? g_settings_get_string(profile->settings, "host") : g_strdup("");
}

/**
 * rm_router_get_login_password:
 * @profile: a #RmProfile
 *
 * Get login password.
 *
 * Returns: login password
 */
gchar *rm_router_get_login_password(RmProfile *profile)
{
	return rm_password_get(profile, "login-password");
}

void rm_router_set_login_password(RmProfile *profile,
                                  char      *password)
{
	rm_password_set(profile, "login-password", password);
}

/**
 * rm_router_get_login_user:
 * @profile: a #RmProfile
 *
 * Get login user
 *
 * Returns: login user
 */
gchar *rm_router_get_login_user(RmProfile *profile)
{
	return g_settings_get_string(profile->settings, "login-user");
}

/**
 * rm_router_get_ftp_password:
 * @profile: a #RmProfile
 *
 * Get router FTP password.
 *
 * Returns: router ftp password
 */
gchar *rm_router_get_ftp_password(RmProfile *profile)
{
	return rm_password_get(profile, "ftp-password");
}

/**
 * rm_router_get_ftp_user:
 * @profile: a #RmProfile
 *
 * Get router FTP user.
 *
 * Returns: router ftp user
 */
gchar *rm_router_get_ftp_user(RmProfile *profile)
{
	return g_settings_get_string(profile->settings, "ftp-user");
}

/**
 * rm_router_get_international_access_code:
 * @profile: a #RmProfile
 *
 * Get international access code
 *
 * Returns: international access code
 */
gchar *rm_router_get_international_access_code(RmProfile *profile)
{
	if (!profile || !profile->settings) {
		return NULL;
	}

	return g_settings_get_string(profile->settings, "international-access-code");
}

/**
 * rm_router_get_national_prefix:
 * @profile: a #RmProfile
 *
 * Get national call prefix
 *
 * Returns: national call prefix
 */
gchar *rm_router_get_national_prefix(RmProfile *profile)
{
	if (!profile || !profile->settings) {
		return NULL;
	}

	return g_settings_get_string(profile->settings, "national-access-code");
}

/**
 * rm_router_get_area_code:
 * @profile: a #RmProfile
 *
 * Get router area code
 *
 * Returns: area code
 */
gchar *rm_router_get_area_code(RmProfile *profile)
{
	if (!profile || !profile->settings) {
		return NULL;
	}

	return g_settings_get_string(profile->settings, "area-code");
}

/**
 * rm_router_get_country_code:
 * @profile: a #RmProfile
 *
 * Get router country code
 *
 * Returns: country code
 */
gchar *rm_router_get_country_code(RmProfile *profile)
{
	if (!profile || !profile->settings) {
		return NULL;
	}

	return g_settings_get_string(profile->settings, "country-code");
}

/**
 * rm_router_get_settings:
 * @profile: a #RmProfile
 *
 * Get router settings (and setup devices)
 *
 * Returns: router settings
 */
gboolean rm_router_get_settings(RmProfile *profile)
{
	gboolean ret = active_router ? active_router->get_settings(profile) : FALSE;

	/*************************************************************/
	/** Temporary workaround to be transparent to the client app */
	/*************************************************************/
	if (ret == TRUE) {
		gchar **numbers;

		/* Store router serial number for detection purpose */
		g_settings_set_string(profile->settings, "serial-number", profile->router_info->serial);

		/* Set initial fax report dir */
		g_settings_set_string(profile->settings, "fax-report-dir", g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS));

		/* Set initial softphone number */
		numbers = rm_router_get_numbers(profile);
		if (numbers && numbers[0]) {
			RmDevice *capi_phone = rm_device_get("CAPI");
			RmDevice *dialer_phone = rm_device_get("Call Monitor");
			gchar **dialer_numbers = rm_strv_remove(numbers, numbers[0]);
			gchar **capi_numbers = rm_strv_add(NULL, numbers[0]);
			const gchar *name = rm_profile_get_name(profile);
			gchar *fax_msn = g_settings_get_string(profile->settings, "fax-number");

			if (g_strcmp0(numbers[0], fax_msn)) {
				gchar **tmp = capi_numbers;

				capi_numbers = rm_strv_add(capi_numbers, fax_msn);
				g_strfreev(tmp);

				tmp = dialer_numbers;
				dialer_numbers = rm_strv_remove(dialer_numbers, fax_msn);
				g_strfreev(tmp);
			}
 
			rm_device_set_numbers(dialer_phone, dialer_numbers, name);
			rm_device_set_numbers(capi_phone, capi_numbers, name);
 
			g_settings_set_string(profile->settings, "phone-number", numbers[0]);
		}
	}

	return ret;
}

/**
 * rm_router_get_name:
 * @profile: a #RmProfile
 *
 * Get router name
 *
 * Returns: router name
 */
const gchar *rm_router_get_name(RmProfile *profile)
{
	if (!profile || !profile->router_info) {
		return NULL;
	}

	return profile->router_info->name;
}

/**
 * rm_router_get_version:
 * @profile: a #RmProfile
 *
 * Get router version
 *
 * Returns: router version
 */
const gchar *rm_router_get_version(RmProfile *profile)
{
	if (!profile || !profile->router_info) {
		return NULL;
	}

	return profile->router_info->version;
}

static void
load_journal_thread (GTask        *task,
                     gpointer     *unused,
                     RmProfile    *profile,
                     GCancellable *cancellable)
{
	GList *list = active_router ? active_router->load_journal(profile) : NULL;

	g_task_return_pointer (task, list, (GDestroyNotify) rm_journal_free);
}

void rm_router_load_journal_async(RmProfile *profile, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task;

	g_assert (profile);
	g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_priority (task, G_PRIORITY_DEFAULT);
	g_task_set_source_tag (task, rm_router_load_journal_async);
	g_task_set_task_data (task, profile, NULL);
	g_task_run_in_thread (task, (GTaskThreadFunc)load_journal_thread);
	g_object_unref (task);
}

gpointer rm_router_load_journal_finish(GObject *source, GAsyncResult *result, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, source), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}


/**
 * rm_router_clear_journal:
 * @profile: a #RmProfile
 *
 * Clear router journal
 *
 * Returns: clear journal return state
 */
gboolean rm_router_clear_journal(RmProfile *profile)
{
	return active_router->clear_journal(profile);
}

/**
 * rm_router_dial_number:
 * @profile: a #RmProfile
 * @port: dial port
 * @number: number to dial
 *
 * Dial number
 *
 * Returns: return state of dial function
 */
gboolean rm_router_dial_number(RmProfile *profile, gint port, const gchar *number)
{
	gchar *target = rm_number_canonize(number);
	gboolean ret;

	ret = active_router->dial_number(profile, port, target);
	g_free(target);

	return ret;
}

/**
 * rm_router_hangup:
 * @profile: a #RmProfile
 * @port: dial port
 * @number: number to dial
 *
 * Hangup call
 *
 * Returns: return state of hangup function
 */
gboolean rm_router_hangup(RmProfile *profile, gint port, const gchar *number)
{
	return active_router->hangup(profile, port, number);
}

/**
 * rm_router_register:
 * @router: a #RmRouter
 *
 * Register new router
 *
 * Returns: %TRUE
 */
gboolean rm_router_register(RmRouter *router)
{
	rm_router_list = g_list_prepend(rm_router_list, router);

	return TRUE;
}

/**
 * rm_router_init:
 *
 * Initialize router (if available set internal router structure)
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean rm_router_init(void)
{
	if (g_list_length(rm_router_list)) {
		return TRUE;
	}

	g_warning("No router plugin registered!");
	return FALSE;
}

/**
 * rm_router_shutdown:
 *
 * Shutdown router
 */
void rm_router_shutdown(void)
{
	/* Free router list */
	if (rm_router_list != NULL) {
		g_list_free(rm_router_list);
		rm_router_list = NULL;
	}

	/* Unset active router */
	active_router = NULL;
}

/**
 * rm_router_process_journal:
 * @journal: journal list
 *
 * Router needs to process a new loaded journal (emit journal-process signal and journal-loaded)
 */
void rm_router_process_journal(GList *journal)
{
	GList *list;

	/* Parse offline journal and combine new entries */
	journal = rm_journal_load(journal);

	/* Store it back to disk */
	rm_journal_save(journal);

	/* Try to lookup entries in address book */
	for (list = journal; list; list = list->next) {
		RmCallEntry *call = list->data;

		rm_object_emit_contact_process(call->remote);
	}
}

/**
 * rm_router_load_fax:
 * @profile: a #RmProfile
 * @name: fax filename
 * @len: pointer to store data length to
 *
 * Load fax file
 *
 * Returns: fax data
 */
gchar *rm_router_load_fax(RmProfile *profile, const gchar *name, gsize *len)
{
	return active_router->load_fax(profile, name, len);
}

/**
 * rm_router_load_voice:
 * @profile: a #RmProfile
 * @name: voice filename
 * @len: pointer to store data length to
 *
 * Load voice file
 *
 * Returns: voice data
 */
gchar *rm_router_load_voice(RmProfile *profile, const gchar *name, gsize *len)
{
	return active_router->load_voice(profile, name, len);
}

typedef struct {
	RmProfile *profile;
	char      *name;
} VoiceMailAsyncData;

static VoiceMailAsyncData *voice_mail_async_data_new (RmProfile *profile, const char *name)
{
	VoiceMailAsyncData *data;

	data = g_new0 (VoiceMailAsyncData, 1);
	data->profile = profile;
	data->name = g_strdup (name);

	return data;
}

static void
voice_mail_async_data_free (VoiceMailAsyncData *data)
{
	g_clear_pointer (&data->name, g_free);

	g_free (data);
}

static void
load_voice_mail_thread (GTask              *task,
                        gpointer           *unused,
                        VoiceMailAsyncData *data,
                        GCancellable       *cancellable)
{
	gsize len;
	char *bytes = rm_router_load_voice (data->profile, data->name, &len);

	g_task_return_pointer (task, g_bytes_new (bytes, len), NULL);
}

void rm_router_load_voice_mail_async(RmProfile *profile, const char *name, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task;

	g_assert (profile);
	g_assert (name);
	g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_priority (task, G_PRIORITY_DEFAULT);
	g_task_set_source_tag (task, rm_router_load_voice_mail_async);
	g_task_set_task_data (task, voice_mail_async_data_new (profile, name), (GDestroyNotify)voice_mail_async_data_free);
	g_task_run_in_thread (task, (GTaskThreadFunc)load_voice_mail_thread);
	g_object_unref (task);
}

GBytes *rm_router_load_voice_mail_finish(GObject *source, GAsyncResult *result, GError **error)
{
	g_assert (g_task_is_valid (result, source));

	return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * rm_router_get_ip:
 * @profile: a #RmProfile
 *
 * Get external IP address
 *
 * Returns: IP address
 */
gchar *rm_router_get_ip(RmProfile *profile)
{
	return active_router->get_ip(profile);
}

/**
 * rm_router_reconnect:
 * @profile: a #RmProfile
 *
 * Reconnect network connection
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean rm_router_reconnect(RmProfile *profile)
{
	return active_router->reconnect(profile);
}

/**
 * rm_router_delete_fax:
 * @profile: a #RmProfile
 * @filename: fax filename to delete
 *
 * Delete fax file on router
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean rm_router_delete_fax(RmProfile *profile, const gchar *filename)
{
	return active_router->delete_fax(profile, filename);
}

/**
 * rm_router_delete_voice:
 * @profile: a #RmProfile
 * @filename: voice filename to delete
 *
 * Delete voice file on router
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean rm_router_delete_voice(RmProfile *profile, const gchar *filename)
{
	return active_router->delete_voice(profile, filename);
}

/**
 * rm_router_info_free:
 * @info: a #RmRouterInfo
 *
 * Free router info structure
 *
 * Returns: %TRUE if structure is freed, otherwise %FALSE
 */
gboolean rm_router_info_free(RmRouterInfo *info)
{
	if (info) {
		g_free(info->name);
		g_free(info->serial);
		g_free(info->version);
		g_free(info->lang);
		g_free(info->annex);

		g_free(info->host);
		g_free(info->user);
		g_free(info->password);
		g_free(info->session_id);

		if (info->session_timer) {
			g_timer_destroy(info->session_timer);
		}

		g_slice_free(RmRouterInfo, info);

		return TRUE;
	}

	return FALSE;
}

/**
 * rm_router_is_cable:
 * @profile: a #RmProfile
 *
 * Check if router is using cable as annex
 *
 * Returns: %TRUE if cable is used, otherwise %FALSE
 */
gboolean rm_router_is_cable(RmProfile *profile)
{
	gboolean is_cable = FALSE;

	if (!RM_EMPTY_STRING(profile->router_info->annex) && !strcmp(profile->router_info->annex, "Kabel")) {
		is_cable = TRUE;
	}

	return is_cable;
}

/**
 * rm_router_load_fax_reports:
 * @profile: a #RmProfile
 * @journal: journal list pointer
 *
 * Load fax reports and add them to the journal
 *
 * Returns: new journal list with attached fax reports
 */
GList *rm_router_load_fax_reports(RmProfile *profile, GList *journal)
{
	g_autoptr (GDir) dir = NULL;
	GError *error = NULL;
	const gchar *file_name;
	gchar *dir_name = g_settings_get_string(profile->settings, "fax-report-dir");

	if (!dir_name) {
		return journal;
	}

	dir = g_dir_open(dir_name, 0, &error);
	if (!dir) {
		g_debug("Could not open fax report directory");
		return journal;
	}

	while ((file_name = g_dir_read_name(dir))) {
		RmCallEntry *call;
		gchar *uri;
		gchar **split;
		gchar *date_time;

		if (strncmp(file_name, "fax-report", 10)) {
			continue;
		}

		split = g_strsplit(file_name, "_", -1);
		if (g_strv_length(split) != 9) {
			g_strfreev(split);
			continue;
		}

		uri = g_build_filename(dir_name, file_name, NULL);

		date_time = g_strdup_printf("%s.%s.%s %2.2s:%2.2s", split[3], split[4], split[5] + 2, split[6], split[7]);

		call = rm_call_entry_new(RM_CALL_ENTRY_TYPE_FAX_REPORT, date_time, "", split[2], ("Fax-Report"), split[1], "0:01", g_strdup(uri));
		journal = rm_journal_add_call_entry(journal, call);

		g_free(uri);
		g_strfreev(split);
	}

	return journal;
}

/**
 * rm_router_load_voice_records:
 * @profile: a #RmProfile
 * @journal: journal list pointer
 *
 * Load voice records and add them to the journal
 *
 * Returns: new journal list with attached voice records
 */
GList *rm_router_load_voice_records(RmProfile *profile, GList *journal)
{
	g_autoptr(GDir) dir = NULL;
	GError *error = NULL;
	const gchar *file_name;
	const gchar *dir_name = rm_get_user_data_dir();

	if (!dir_name) {
		return journal;
	}

	dir = g_dir_open(dir_name, 0, &error);
	if (!dir) {
		g_debug("%s(): Could not open voice records directory (%s)", __FUNCTION__, dir_name);
		return journal;
	}

	while ((file_name = g_dir_read_name(dir))) {
		RmCallEntry *call;
		gchar *uri;
		gchar **split;
		gchar *date_time;
		gchar *num;

		/* %2.2d.%2.2d.%4.4d-%2.2d-%2.2d-%s-%s.wav",
		        time_val->tm_mday, time_val->tm_mon, 1900 + time_val->tm_year,
		        time_val->tm_hour, time_val->tm_min, connection->source, connection->target);
		 */

		if (!strstr(file_name, ".wav")) {
			continue;
		}

		split = g_strsplit(file_name, "-", -1);
		if (g_strv_length(split) != 5) {
			g_strfreev(split);
			continue;
		}

		uri = g_strdup_printf("file:///%s%s%s", dir_name, G_DIR_SEPARATOR_S, file_name);
		num = split[4];
		num[strlen(num) - 4] = '\0';

		//date_time = g_strdup_printf("%s.%s.%s %2.2s:%2.2s", split[3], split[4], split[5] + 2, split[6], split[7]);
		date_time = g_strdup_printf("%s %2.2s:%2.2s", split[0], split[1], split[2]);

		call = rm_call_entry_new(RM_CALL_ENTRY_TYPE_RECORD, date_time, "", num, ("Record"), split[3], "0:01", g_strdup(uri));
		journal = rm_journal_add_call_entry(journal, call);

		g_free(uri);
		g_strfreev(split);
	}

	return journal;
}

/**
 * rm_router_get_suppress_state:
 * @profile: a #RmProfile
 *
 * Get number suppress state
 *
 * Returns: suppress state
 */
gboolean rm_router_get_suppress_state(RmProfile *profile)
{
	return g_settings_get_boolean(profile->settings, "suppress");
}

/**
 * rm_router_need_ftp:
 * @profile: a #RmProfile
 *
 * Checks wether router needs ftp or is capable of TR-064
 *
 * Returns: %TRUE if ftp support is needed
 */
gboolean rm_router_need_ftp(RmProfile *profile)
{
	return active_router ? active_router->need_ftp(profile) : TRUE;
}
