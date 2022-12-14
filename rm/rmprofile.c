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

#include <rm/rmaction.h>
//#include <rm/fax_phone.h>
#include <rm/rmprofile.h>
#include <rm/rmplugins.h>
#include <rm/rmmain.h>
#include <rm/rmrouter.h>
#include <rm/rmnetmonitor.h>
#include <rm/rmpassword.h>
#include <rm/rmobjectemit.h>
#include <rm/rmaudio.h>
#include <rm/rmsettings.h>
#include <rm/rmnotification.h>
#include <rm/rmfilter.h>
#include <rm/rmstring.h>

/**
 * SECTION:rmprofile
 * @Title: RmProfile
 * @Short_description: Profile handling functions
 *
 * Profiles are used to distinguish between several routers. This is useful for mobile devices using private and business environements.
 *
 * The strategy should always be:
 *  - Return set device
 *  - If it is not present, use first available plugin and set it as default
 *  - Only if no alternative is found, return %NULL
 */

/** Internal profile list */
static GList *rm_profile_list = NULL;
/** Internal pointer to active profile */
static RmProfile *rm_profile_active = NULL;
/** Global application settings */
static GSettings *rm_settings = NULL;

/**
 * rm_profile_get_name:
 * @profile: a #RmProfile
 *
 * Get profiles name of @profile.
 *
 * Returns: profile name
 */
const gchar *rm_profile_get_name(RmProfile *profile)
{
	return profile->name;
}

/**
 * rm_profile_save:
 *
 * Update profiles in gsettting.
 */
static void rm_profile_save(void)
{
	GList *plist = rm_profile_get_list();
	GList *list;
	gchar **profiles;
	gint counter = 0;

	profiles = g_new0(gchar *, (g_list_length(plist) + 1));
	for (list = plist; list != NULL; list = list->next) {
		RmProfile *current_profile = list->data;

		profiles[counter++] = g_strdup(rm_profile_get_name(current_profile));
	}
	profiles[counter] = NULL;

	g_settings_set_strv(rm_settings, "profiles", (const gchar*const*)profiles);

	g_strfreev(profiles);
}

/**
 * rm_profile_remove:
 * @profile: a #RmProfile
 *
 * Remove @profile from list.
 */
void rm_profile_remove(RmProfile *profile)
{
	rm_profile_list = g_list_remove(rm_profile_list, profile);

	if (profile->settings) {
		g_object_unref(profile->settings);
	}

	rm_router_info_free(profile->router_info);

	g_free(profile->name);

	/* Remove gsettings entry */
	rm_profile_save();
}

/**
 * rm_profile_get_active:
 *
 * Get active profile.
 *
 * Returns: active #RmProfile
 */
RmProfile *rm_profile_get_active(void)
{
	return rm_profile_active;
}

/**
 * rm_profile_get_list:
 *
 * Get profile list
 *
 * Returns: profile list
 */
GList *rm_profile_get_list(void)
{
	return rm_profile_list;
}

/**
 * rm_profile_add:
 * @name: profile name
 *
 * Create and add a new profile structure.
 *
 * Returns: new #RmProfile
 */
RmProfile *rm_profile_add(const gchar *name)
{
	RmProfile *profile;
	gchar *settings_path;

	/* Create new profile structure */
	profile = g_slice_new0(RmProfile);

	/* Add entries */
	profile->name = g_strdup(name);
	profile->router_info = g_slice_new0(RmRouterInfo);

	/* Setup profiles settings */
	settings_path = g_strconcat("/org/tabos/rm/", name, "/", NULL);
	profile->settings = rm_settings_new_with_path(RM_SCHEME_PROFILE, settings_path);

	g_free(settings_path);

	rm_profile_list = g_list_prepend(rm_profile_list, profile);

	rm_profile_save();

	/* Return new profile */
	return profile;
}

/**
 * rm_profile_load:
 * @name: profile name to load
 *
 * Load profile by name.
 */
static void rm_profile_load(const gchar *name)
{
	RmProfile *profile;
	gchar *settings_path;

	profile = g_slice_new0(RmProfile);

	profile->name = g_strdup(name);

	settings_path = g_strconcat("/org/tabos/rm/", name, "/", NULL);
	profile->settings = rm_settings_new_with_path(RM_SCHEME_PROFILE, settings_path);

	profile->router_info = g_slice_new0(RmRouterInfo);
	profile->router_info->host = g_settings_get_string(profile->settings, "host");
	profile->router_info->user = g_settings_get_string(profile->settings, "login-user");
	profile->router_info->password = rm_password_get(profile, "login-password");

	g_free(settings_path);

	rm_profile_list = g_list_prepend(rm_profile_list, profile);
}

static gboolean rm_profile_set_active_idle(gpointer user_data)
{
	RmProfile *profile = user_data;

	rm_router_set_active(profile);

	rm_plugins_bind_loaded_plugins();

	/* Load and initialize action */
	rm_action_init(profile);

	/* Load and initialize filters */
	rm_filter_init(profile);

	rm_object_emit_profile_changed ();
	return G_SOURCE_REMOVE;
}

/**
 * rm_profile_set_active:
 * @profile: a #RmProfile
 *
 * Set active profile in detail:
 *  * Set internal active profile
 *  * Connect user plugin bindings
 *  * Initialize audio
 *  * Load and initialize action
 *  * Load journal
 */
void rm_profile_set_active(RmProfile *profile)
{
	if (rm_profile_active == profile) {
		return;
	}

	/* Shut profile actions down */
	rm_action_shutdown(rm_profile_active);

	/* Shut profile filter down */
	rm_filter_shutdown(rm_profile_active);

	rm_profile_active = profile;

	/* If we have no active profile, exit */
	if (!rm_profile_active) {
		return;
	}

	g_idle_add (rm_profile_set_active_idle, profile);
}

/**
 * rm_profile_detect:
 *
 * Detect profile (scan available routers).
 *
 * Returns: #RmProfile or %NULL if none found
 */
RmProfile *rm_profile_detect(void)
{
	GList *list = rm_profile_get_list();
	RmProfile *profile;
	RmRouterInfo *router_info;
	gchar *requested_profile = rm_get_requested_profile();

	/* Compare our profile list against available routers */
	while (list) {
		profile = list->data;

		/* Check whether user specified a profile */
		if (requested_profile && strcmp(profile->name, requested_profile)) {
			list = list->next;
			continue;
		}

		router_info = g_slice_new0(RmRouterInfo);
		router_info->host = g_settings_get_string(profile->settings, "host");

		/* Check if router is present */
		if (rm_router_present(router_info) == TRUE && !strcmp(router_info->serial, g_settings_get_string(profile->settings, "serial-number"))) {
			g_debug("%s(): Configured router found: %s", __FUNCTION__, router_info->name);
			g_debug("%s(): Current router firmware: %d.%d.%d", __FUNCTION__, router_info->box_id, router_info->maj_ver_id, router_info->min_ver_id);

			rm_router_info_free(router_info);

			return profile;
		}

		rm_router_info_free(router_info);

		list = list->next;
	}

	g_debug("%s(): No already configured router found!", __FUNCTION__);

	return NULL;
}

/**
 * rm_profile_init:
 *
 * Initialize profiles (load profiles).
 * Returns: TRUE
 */
gboolean rm_profile_init(void)
{
	rm_settings = rm_settings_new(RM_SCHEME);

	gchar **profiles = g_settings_get_strv(rm_settings, "profiles");
	gint count;

	/* Load all available profiles */
	for (count = 0; count < g_strv_length(profiles); count++) {
		rm_profile_load(profiles[count]);
	}

	g_strfreev(profiles);

	return TRUE;
}

/**
 * rm_profile_free_entry:
 * @data: pointer to a #RmProfile
 *
 * Free profile entry.
 */
static void rm_profile_free_entry(gpointer data)
{
	RmProfile *profile = data;

	/* Free entries */
	g_free(profile->name);

	/* Free router information structure */
	rm_router_info_free(profile->router_info);

	/* Free actions */
	rm_action_shutdown(profile);

	/* Free profiles settings */
	g_clear_object(&profile->settings);

	/* Free structure */
	g_slice_free(RmProfile, profile);
}

/**
 * rm_profile_shutdown:
 *
 * Shutdown profile (free profile list, free settings).
 */
void rm_profile_shutdown(void)
{
	/* Set active profile to NULL */
	rm_profile_active = NULL;

	if (rm_profile_list) {
		/* Free profile list */
		g_list_free_full(rm_profile_list, rm_profile_free_entry);
	}

	/* Clear settings object */
	if (rm_settings) {
		g_clear_object(&rm_settings);
	}
}

/**
 * rm_profile_set_host:
 * @profile: a #RmProfile
 * @host: new host name for @profile
 *
 * Set host name used in profile.
 */
void rm_profile_set_host(RmProfile *profile, const gchar *host)
{
	if (profile->router_info->host) {
		g_free(profile->router_info->host);
	}

	profile->router_info->host = g_strdup(host);
	g_settings_set_string(profile->settings, "host", host);
}

/**
 * rm_profile_set_login_user:
 * @profile: a #RmProfile
 * @user: new user name for @profile
 *
 * Set login user used in profile.
 */
void rm_profile_set_login_user(RmProfile *profile, const gchar *user)
{
	g_settings_set_string(profile->settings, "login-user", user);
}

/**
 * rm_profile_set_login_password:
 * @profile: a #RmProfile
 * @password: new password for @profile
 *
 * Set login password used in profile.
 */
void rm_profile_set_login_password(RmProfile *profile, const gchar *password)
{
	rm_password_set(profile, "login-password", password);
}

/**
 * rm_profile_get_addressbook:
 * @profile: a #RmProfile
 *
 * Get address book for selected profile.
 *
 * Returns: a #RmAddressBook
 */
RmAddressBook *rm_profile_get_addressbook(RmProfile *profile)
{
	RmAddressBook *book = NULL;
	gchar *name = g_settings_get_string(profile->settings, "address-book-plugin");

	if (!RM_EMPTY_STRING(name)) {
		book = rm_addressbook_get(name);
	} else {
		GList *books = rm_addressbook_get_plugins();

		if (books) {
			g_debug("%s(): Address book not set, setting it to first one", __FUNCTION__);
			book = books->data;
			g_settings_set_string(profile->settings, "address-book-plugin", rm_addressbook_get_name(book));
		}
	}

	g_free(name);

	return book;
}

/**
 * rm_profile_set_addressbook:
 * @profile: a #RmProfile
 * @book: a #RmAddressBook
 *
 * Sets preferred addressbook within profile.
 */
void rm_profile_set_addressbook(RmProfile *profile, RmAddressBook *book)
{
	g_settings_set_string(profile->settings, "address-book-plugin", rm_addressbook_get_name(book));
}

/**
 * rm_profile_get_audio:
 * @profile: a #RmProfile
 *
 * Get audio for selected profile.
 */
RmAudio *rm_profile_get_audio(RmProfile *profile)
{
	RmAudio *audio;
	gchar *name = g_settings_get_string(profile->settings, "audio-plugin");

	audio = rm_audio_get(name);

	g_free(name);

	return audio;
}

/**
 * rm_profile_get_audio_ringtone:
 * @profile: a #RmProfile
 *
 * Get ringtone audio device for selected profile.
 *
 * Returns: Audio ringtone device
 */
gchar *rm_profile_get_audio_ringtone(RmProfile *profile)
{
	return g_settings_get_string(profile->settings, "audio-output-ringtone");
}

/**
 * rm_profile_get_notification:
 * @profile: a #RmProfile
 *
 * Get notification for selected profile.
 *
 * Returns: a #RmNotification
 */
RmNotification *rm_profile_get_notification(RmProfile *profile)
{
	RmNotification *notification;
	gchar *name = g_settings_get_string(profile->settings, "notification-plugin");

	notification = rm_notification_get(name);
	if (!notification) {
		GList *notifications = rm_notification_get_plugins();

		if (notifications) {
			notification = notifications->data;

			g_debug("%s(): No notification set, using first one %s", __FUNCTION__, rm_notification_get_name(notification));
		}
	}

	g_free(name);

	return notification;
}

/**
 * rm_profile_get_notification_ringtone:
 * @profile: a #RmProfile
 *
 * Get notification ringtone setting for selected profile.
 */
gboolean rm_profile_get_notification_ringtone(RmProfile *profile)
{
	return g_settings_get_boolean(profile->settings, "notification-play-ringtone");
}

/**
 * rm_profile_get_notification_incoming_numbers:
 * @profile: a #RmProfile
 *
 * Get notification incoming numbers for selected profile.
 */
gchar **rm_profile_get_notification_incoming_numbers(RmProfile *profile)
{
	return g_settings_get_strv(profile->settings, "notification-incoming-numbers");
}

/**
 * rm_profile_get_notification_outgoing_numbers:
 * @profile: a #RmProfile
 *
 * Get notification outgoing numbers for selected profile.
 */
gchar **rm_profile_get_notification_outgoing_numbers(RmProfile *profile)
{
	return g_settings_get_strv(profile->settings, "notification-outgoing-numbers");
}

/**
 * rm_profile_set_notification_incoming_numbers:
 * @profile: a #RmProfile
 * @numbers: numbers to set
 *
 * Set notification incoming numbers for selected profile.
 */
void rm_profile_set_notification_incoming_numbers(RmProfile *profile, const gchar * const* numbers)
{
	g_settings_set_strv(profile->settings, "notification-incoming-numbers", numbers);
}

/**
 * rm_profile_set_notification_outgoing_numbers:
 * @profile: a #RmProfile
 * @numbers: numbers to set
 *
 * Set notification outgoing numbers for selected profile.
 */
void rm_profile_set_notification_outgoing_numbers(RmProfile *profile, const gchar * const* numbers)
{
	g_settings_set_strv(profile->settings, "notification-outgoing-numbers", numbers);
}

/**
 * rm_profile_get_phone:
 * @profile: a #RmProfile
 *
 * Get phone for selected profile.
 */
RmPhone *rm_profile_get_phone(RmProfile *profile)
{
	RmPhone *phone;
	gchar *name = g_settings_get_string(profile->settings, "phone-plugin");

	phone = rm_phone_get(name);

	g_free(name);

	if (!phone) {
		GList *phones = rm_phone_get_plugins();

		if (phones) {
			phone = phones->data;

			g_debug("%s(): No phone set, using first one %s", __FUNCTION__, rm_phone_get_name(phone));
		}
	}

	return phone;
}

/**
 * rm_profile_set_phone:
 * @profile: a #RmProfile
 * @phone: a #RmPhone
 *
 * Set preferred phone within profile.
 */
void rm_profile_set_phone(RmProfile *profile, RmPhone *phone)
{
	g_settings_set_string(profile->settings, "phone-plugin", rm_phone_get_name (phone));
}

/**
 * rm_profile_get_fax:
 * @profile: a #RmProfile
 *
 * Get fax for selected profile.
 */
RmFax *rm_profile_get_fax(RmProfile *profile)
{
	RmFax *fax;
	gchar *name = g_settings_get_string(profile->settings, "fax-plugin");

	fax = rm_fax_get(name);

	g_free(name);

	return fax;
}

/**
 * rm_profile_set_fax:
 * @profile: a #RmProfile
 * @name: fax name
 *
 * Set preferred phone name within profile.
 */
void rm_profile_set_fax(RmProfile *profile, gchar *name)
{
	g_settings_set_string(profile->settings, "fax-plugin", name);
}

/**
 * rm_profile_update_numbers:
 * @profile: a #RmProfile
 * 
 * Updates numbers for each #RmDevice (currently only CAPI and Dialer)
 * This one is hard-coded as we are currently supporting CAPI and Call Monitor only, once
 * SIP is implemented we need to break this one up
 */
void rm_profile_update_numbers(RmProfile *profile)
{
	RmDevice *capi_phone = rm_device_get("CAPI");
	RmDevice *dialer_phone = rm_device_get("Call Monitor");
	const gchar *profile_name = rm_profile_get_name(profile);
	gchar **dialer_numbers = rm_router_get_numbers(profile);
	gchar **capi_numbers = NULL;
	gchar *phone_msn = g_settings_get_string(profile->settings, "phone-number");
	gchar *fax_msn = g_settings_get_string(profile->settings, "fax-number");

	if (g_settings_get_boolean(profile->settings, "fax-active")) {
			gchar **tmp;

			g_debug("%s(): Fax is active, adding '%s'", __FUNCTION__, fax_msn);
			capi_numbers = rm_strv_add(capi_numbers, fax_msn);

			tmp = dialer_numbers;
			dialer_numbers = rm_strv_remove(dialer_numbers, fax_msn);
			g_strfreev(tmp);
	}

	if (g_settings_get_boolean(profile->settings, "phone-active")) {
			gchar **tmp;

			g_debug("%s(): Phone is active, adding '%s'", __FUNCTION__, phone_msn);
			tmp = capi_numbers;
			capi_numbers = rm_strv_add(capi_numbers, phone_msn);
			g_strfreev(tmp);

			tmp = dialer_numbers;
			dialer_numbers = rm_strv_remove(dialer_numbers, phone_msn);
			g_strfreev(tmp);
	}

	rm_device_set_numbers(dialer_phone, dialer_numbers, profile_name);
	rm_device_set_numbers(capi_phone, capi_numbers, profile_name);
}

