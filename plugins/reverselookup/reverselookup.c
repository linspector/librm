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

#include <rmconfig.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <glib.h>

#include <rm/rm.h>

#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

//#define RL_DEBUG 1

typedef struct {
	guint signal_id;
	GHashTable *table;
} RmReverseLookupPlugin;

static GHashTable *table = NULL;
/** Global lookup list */
static GSList *lookup_list = NULL;
/** Lookup country code hash table */
static GHashTable *lookup_table = NULL;
/** Lookup soup session */
static SoupSession *rl_session = NULL;

typedef struct _RmLookupEntry {
	gboolean prefix;
	gchar *service;
	gchar *url;
	gchar **name;
	gchar **street;
	gchar **zip;
	gchar **city;
	gint zip_len;
} RmLookupEntry;

/**
 * reverselookup_replace_number:
 * @url: url string
 * @full_number: full phone number
 *
 * Repleaces %NUMBER% within @url with @full_number.
 *
 * Returns: replaced string
 */
static gchar *reverselookup_replace_number(gchar *url, gchar *full_number)
{
	GRegex *number = g_regex_new("%NUMBER%", G_REGEX_DOTALL | G_REGEX_OPTIMIZE, 0, NULL);
	gchar *out = g_regex_replace_literal(number, url, -1, 0, full_number, 0, NULL);

	g_regex_unref(number);

	return out;
}

/**
 * reversellokup_extract_element:
 * @in: input string
 * @a_node: a #xmlNode
 * @out: output string
 *
 * Extracts a defined xml element of an xmlNode
 *
 * Returns: %TRUE if element could be extracted
 */
static gboolean reverselookup_extract_element(gchar **in, xmlNode *a_node, gchar **out)
{
	xmlNode *cur_node = NULL;
	gboolean ret = FALSE;

	for (cur_node = a_node; cur_node && !ret; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			if (!strcmp((gchar*)cur_node->name, in[0])) {
				gchar *type = (gchar*)xmlGetProp(cur_node, (xmlChar*)in[1]);

				if (type && !strcmp(type, in[2])) {
					gchar *tmp;
					xmlChar *name = xmlNodeListGetString(cur_node->doc, cur_node->children, TRUE);
					GRegex *whitespace = g_regex_new("[ ]{2,}", G_REGEX_DOTALL | G_REGEX_OPTIMIZE, 0, NULL);

					tmp = g_regex_replace_literal(whitespace, (gchar*)name, -1, 0, " ", 0, NULL);

					*out = g_strdup(g_strstrip(tmp));
					g_free(tmp);

					xmlFree(name);
					return TRUE;
				}
			}
		}

		ret = reverselookup_extract_element(in, cur_node->children, out);
	}

	return ret;
}

/**
 * reverselookup_do_entry:
 * @lookup: a #RmLookupEntry
 * @number: number to lookup
 * @contact: a #RmContact
 *
 * Internal reverse lookup function
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
static gboolean reverselookup_do_entry(RmLookupEntry *lookup, gchar *number, RmContact *contact)
{
	SoupMessage *msg;
	const gchar *data;
	gchar *rl_tmp;
	RmContact *rl_contact;
	gboolean result = FALSE;
	gchar *full_number;
	gchar *rdata = NULL;
	gchar *file;
	gsize len;

	/* get full number according to service preferences */
	full_number = rm_number_full(number, lookup->prefix);

#ifdef RL_DEBUG
	g_debug("New lookup for '%s'", full_number);
#endif

	gchar *url = reverselookup_replace_number(lookup->url, full_number);
#ifdef RL_DEBUG
	g_debug("URL: %s", url);
#endif
	SoupURI *uri = soup_uri_new(url);
	msg = soup_message_new_from_uri(SOUP_METHOD_GET, uri);
	soup_message_headers_append(msg->request_headers, "User-Agent", "Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.2; Trident/6.0)");

	soup_session_send_message(rl_session, msg);
	soup_uri_free(uri);
	g_free(url);
	if (msg->status_code != 200) {
		//g_debug("Service: %s received status code: %d", lookup->service, msg->status_code);
		g_object_unref(msg);
		return FALSE;
	}

	data = msg->response_body->data;
	len = msg->response_body->length;
	if (!len) {
		goto end;
	}

	rdata = rm_convert_utf8(data, len);

	htmlDocPtr html;
	xmlNodePtr node;

	html = htmlReadMemory(data, len, lookup->url, "utf-8", HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET);

	node = xmlDocGetRootElement(html);

	if (!reverselookup_extract_element(lookup->name, node, &contact->name)) {
#ifdef RL_DEBUG
		gchar *tmp_file = g_strdup_printf("rl-%s-%s.html", lookup->service, number);
		rm_log_save_data(tmp_file, rdata, len);
		g_free(tmp_file);
		g_debug("%s(): Could not extract name", __FUNCTION__);
#endif
		goto end;
	}

	if (!reverselookup_extract_element(lookup->street, node, &contact->street)) {
#ifdef RL_DEBUG
		gchar *tmp_file = g_strdup_printf("rl-%s-%s.html", lookup->service, number);
		rm_log_save_data(tmp_file, rdata, len);
		g_free(tmp_file);
		g_debug("%s(): Could not extract street", __FUNCTION__);
#endif
		goto end;
	}

	if (!reverselookup_extract_element(lookup->city, node, &contact->city)) {
#ifdef RL_DEBUG
		gchar *tmp_file = g_strdup_printf("rl-%s-%s.html", lookup->service, number);
		rm_log_save_data(tmp_file, rdata, len);
		g_free(tmp_file);
		g_debug("%s(): Could not extract city", __FUNCTION__);
#endif
		goto end;
	}

	if (lookup->zip) {
		if (!reverselookup_extract_element(lookup->zip, node, &contact->zip)) {
	#ifdef RL_DEBUG
			gchar *tmp_file = g_strdup_printf("rl-%s-%s.html", lookup->service, number);
			rm_log_save_data(tmp_file, rdata, len);
			g_free(tmp_file);
			g_debug("%s(): Could not extract zip", __FUNCTION__);
	#endif
			goto end;
		}
	} else if (lookup->zip_len) {
		contact->zip = g_strndup(contact->city, lookup->zip_len);
		memmove(contact->city, contact->city + lookup->zip_len + 1, strlen(contact->city) - lookup->zip_len + 1);
	}

	rl_contact = g_slice_new0(RmContact);
	rl_contact->name = g_strdup(contact->name);
	rl_contact->street = g_strdup(contact->street);
	rl_contact->zip = g_strdup(contact->zip);
	rl_contact->city = g_strdup(contact->city);
	g_hash_table_insert(table, number, rl_contact);
	result = TRUE;

	rl_tmp = g_strdup_printf("%s;%s;%s;%s;%s\n",
				 number,
				 rl_contact->name,
				 rl_contact->street,
				 rl_contact->zip,
				 rl_contact->city);

	file = g_build_filename(rm_get_user_cache_dir(), "reverselookup", number, NULL);
	rm_file_save(file, rl_tmp, strlen(rl_tmp));
	g_free(file);
	g_free(rl_tmp);

#ifdef RL_DEBUG
	gchar *tmp_file = g_strdup_printf("rl-found-%s-%s.html", lookup->service, number);
	rm_log_save_data(tmp_file, rdata, len);
	g_free(tmp_file);
#endif

 end:
	if (rdata) {
		g_free(rdata);
	}

	g_object_unref(msg);

	return result;
}

/**
 * reverselookup_get_lookup_list:
 * @country_code: country code
 *
 * Get lookup list
 *
 * Returns: lookup list
 */
static GSList *reverselookup_get_lookup_list(const gchar *country_code)
{
	GSList *list = NULL;

	/* If country code is NULL, return current lookup list */
	if (!country_code) {
		return lookup_list;
	}

	if (!lookup_table) {
		return NULL;
	}

	/* Extract country code list from hashtable */
	list = g_hash_table_lookup(lookup_table, (gpointer)atol(country_code));

	return list;
}

/**
 * reverselookup_get_country_code:
 * @full_number: full international number
 *
 * Extract country code from full number
 *
 * Returns: country code or %NULL on error
 */
static gchar *reverselookup_get_country_code(const gchar *full_number)
{
	gchar sub_string[7];
	int index;
	int len = strlen(full_number);

	for (index = 6; index > 0; index--) {
		if (len <= index) {
			continue;
		}

		strncpy(sub_string, full_number, index);
		sub_string[index] = '\0';

		if (g_hash_table_lookup(lookup_table, (gpointer)atol(sub_string))) {
			return g_strdup(sub_string);
		}
	}

	return NULL;
}

/**
 * reverselookup_do:
 * @number: number to lookup
 * @contact: a #RmContact
 *
 * Reverse lookup function
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
static gboolean reverselookup_do(gchar *number, RmContact *contact)
{
	RmLookupEntry *lookup = NULL;
	GSList *list = NULL;
	gchar *full_number = NULL;
	gchar *country_code = NULL;
	gboolean found = FALSE;
	gint international_access_code_len;
	RmProfile *profile = rm_profile_get_active();
	RmContact *rl_contact;

	if (!profile) {
		return FALSE;
	}

	/* In case we do not have a number, abort */
	if (RM_EMPTY_STRING(number) || !isdigit(number[0])) {
		return FALSE;
	}

#ifdef RL_DEBUG
	g_debug("Input number '%s'", number);
#endif

	rl_contact = g_hash_table_lookup(table, number);
	if (rl_contact) {
		if (!RM_EMPTY_STRING(rl_contact->name)) {
			contact->name = g_strdup(rl_contact->name);
			contact->street = g_strdup(rl_contact->street);
			contact->zip = g_strdup(rl_contact->zip);
			contact->city = g_strdup(rl_contact->city);
			return TRUE;
		}

		return FALSE;
	}

	/* Get full number and extract country code if possible */
	full_number = rm_number_full(number, TRUE);
	if (!full_number) {
		return FALSE;
	}

#ifdef RL_DEBUG
	g_debug("full number '%s'", full_number);
#endif

	country_code = reverselookup_get_country_code(full_number);
	g_free(full_number);

	international_access_code_len = strlen(rm_router_get_international_access_code(profile));
#ifdef RL_DEBUG
	if (!country_code) {
		g_debug("Warning: Could not get country code!!");
	} else {
		g_debug("Country code: %s", country_code + international_access_code_len);
	}
#endif

	if (!country_code) {
		return FALSE;
	}

	if (strcmp(country_code + international_access_code_len, rm_router_get_country_code(rm_profile_get_active()))) {
		/* if country code is not the same as the router country code, loop through country list */
		list = reverselookup_get_lookup_list(country_code + international_access_code_len);
	} else {
		/* if country code is the same as the router country code, use default plugin */
		list = reverselookup_get_lookup_list(rm_router_get_country_code(rm_profile_get_active()));
	}

	g_free(country_code);

	for (; list != NULL && list->data != NULL; list = list->next) {
		lookup = list->data;

#ifdef RL_DEBUG
		g_debug("Using service '%s'", lookup->service);
#endif

		found = reverselookup_do_entry(lookup, number, contact);
		/* in case we found some valid data, break loop */
		if (found) {
			break;
		}
	}

	if (!found) {
		rl_contact = g_slice_new0(RmContact);

		g_hash_table_insert(table, number, rl_contact);
	}

	return found;
}

/**
 * reverselookup_add:
 * @node: a #RmXmlNode
 *
 * Add lookup from xmlnode
 */
static void reverselookup_add(RmXmlNode *node)
{
	RmLookupEntry *lookup = NULL;
	RmXmlNode *child = NULL;
	gchar *service = NULL;
	gchar *prefix = NULL;
	gchar *url = NULL;
	gchar **name = NULL;
	gchar **street = NULL;
	gchar **city = NULL;
	gchar **zip = NULL;
	gint zip_len = 0;

	child = rm_xmlnode_get_child(node, "service");
	g_assert(child != NULL);
	service = rm_xmlnode_get_data(child);

	child = rm_xmlnode_get_child(node, "prefix");
	g_assert(child != NULL);
	prefix = rm_xmlnode_get_data(child);

	child = rm_xmlnode_get_child(node, "url");
	g_assert(child != NULL);
	url = rm_xmlnode_get_data(child);

	gchar *tmp;

	child = rm_xmlnode_get_child(node, "name");
	g_assert(child != NULL);
	tmp = rm_xmlnode_get_data(child);
	name = g_strsplit(tmp, " ", -1);

	child = rm_xmlnode_get_child(node, "street");
	g_assert(child != NULL);
	tmp = rm_xmlnode_get_data(child);
	street = g_strsplit(tmp, " ", -1);

	child = rm_xmlnode_get_child(node, "city");
	g_assert(child != NULL);
	tmp = rm_xmlnode_get_data(child);
	city = g_strsplit(tmp, " ", -1);

	tmp = (gchar*)rm_xmlnode_get_attrib(child, "zip");
	if (tmp) {
		zip_len = atoi(tmp);
	}

	child = rm_xmlnode_get_child(node, "zip");

	if (child) {
		tmp = rm_xmlnode_get_data(child);
		zip = g_strsplit(tmp, " ", -1);
	}

	lookup = g_slice_alloc0(sizeof(RmLookupEntry));
	g_debug(" o Service: '%s', prefix: %s", service, prefix);
	lookup->service = service;
	lookup->prefix = prefix[ 0 ] == '1';
	lookup->url = url;
	lookup->name = name;
	lookup->street = street;
	lookup->city = city;
	lookup->zip = zip;
	lookup->zip_len = zip_len;

	lookup_list = g_slist_prepend(lookup_list, lookup);
}

/**
 * reverselookup_country_code_add:
 * @node: a #RmXmlNode
 *
 * Add country code from RmXmlNode
 */
static void reverselookup_country_code_add(RmXmlNode *node)
{
	RmXmlNode *child = NULL;
	const gchar *code = NULL;

	code = rm_xmlnode_get_attrib(node, "code");
	g_debug("Country Code: %s", code);

	lookup_list = NULL;

	for (child = rm_xmlnode_get_child(node, "lookup"); child != NULL; child = rm_xmlnode_get_next_twin(child)) {
		reverselookup_add(child);
	}
	lookup_list = g_slist_reverse(lookup_list);

	g_hash_table_insert(lookup_table, (gpointer)atol(code), lookup_list);
}

/**
 * reverselookup_read_cache:
 * @dir_name: read cache file from @dir_name
 *
 * Read cached data (previous lookups with valid data)
 */
static void reverselookup_read_cache(gchar *dir_name)
{
#ifndef RL_DEBUG
	g_autoptr (GDir) dir = NULL;
	GError *error = NULL;
	const gchar *file_name;

	if (!dir_name) {
		return;
	}

	dir = g_dir_open(dir_name, 0, &error);
	if (!dir) {
		g_debug("Could not open lookup directory");
		return;
	}

	while ((file_name = g_dir_read_name(dir))) {
		gchar *data;
		gchar *uri;
		gchar **split;
		RmContact *contact;

		uri = g_build_filename(dir_name, file_name, NULL);
		data = rm_file_load(uri, NULL);
		g_free(uri);

		g_assert(data != NULL);

		split = g_strsplit(data, ";", -1);

		contact = g_slice_new0(RmContact);

		contact->name = g_strdup(split[1]);
		contact->street = g_strdup(split[2]);
		contact->zip = g_strdup(split[3]);
		contact->city = g_strstrip(g_strdup(split[4]));

		g_hash_table_insert(table, g_strdup(split[0]), contact);

		g_free(data);
		g_strfreev(split);
	}
#endif
}

RmLookup rl = {
	"Reverse Lookup",
	reverselookup_do
};

/**
 * reverselookup_plugin_init:
 * @plugin: a #RmPlugin
 *
 * Activate plugin
 *
 * Returns: %TRUE if plugin could be initialized
 */
static gboolean reverselookup_plugin_init(RmPlugin *plugin)
{
	RmReverseLookupPlugin *reverselookup_plugin = g_slice_new0(RmReverseLookupPlugin);
	RmXmlNode *node, *child;
	gchar *file;

	plugin->priv = reverselookup_plugin;

	reverselookup_plugin->table = g_hash_table_new(g_str_hash, g_str_equal);
	table = g_hash_table_new(g_str_hash, g_str_equal);

	file = g_build_filename(g_get_home_dir(), "lookup.xml", NULL);
	if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
		g_free(file);

		file = g_build_filename(rm_get_directory(RM_PLUGINS), "reverselookup", "lookup.xml", NULL);
	}

	node = rm_xml_read_from_file(file);
	if (!node) {
		g_debug("Could not read %s", file);
		g_free(file);
		return FALSE;
	}
	g_debug("ReverseLookup: '%s'", file);
	g_free(file);

	/* Create new lookup hash table */
	lookup_table = g_hash_table_new(NULL, NULL);

	for (child = rm_xmlnode_get_child(node, "country"); child != NULL; child = rm_xmlnode_get_next_twin(child)) {
		/* Add new country code lists to hash table */
		reverselookup_country_code_add(child);
	}

	file = g_build_filename(rm_get_user_cache_dir(), "reverselookup", NULL);
	g_mkdir_with_parents(file, 0770);
	reverselookup_read_cache(file);
	g_free(file);

	rm_xmlnode_free(node);

	rl_session = soup_session_new();

	rm_lookup_register(&rl);

	return TRUE;
}

/**
 * reverselookup_plugin_shutdown:
 * @plugin: a #RmPlugin
 *
 * Deactivate plugin
 *
 * Returns: %TRUE
 */
static gboolean reverselookup_plugin_shutdown(RmPlugin *plugin)
{
	RmReverseLookupPlugin *reverselookup_plugin = plugin->priv;

	/* If signal handler is connected: disconnect */
	if (g_signal_handler_is_connected(G_OBJECT(rm_object), reverselookup_plugin->signal_id)) {
		g_signal_handler_disconnect(G_OBJECT(rm_object), reverselookup_plugin->signal_id);
	}

	rm_lookup_unregister(&rl);

	return TRUE;
}

RM_PLUGIN(reverselookup)
