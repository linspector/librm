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

#ifndef __RM_PLUGINS_H__
#define __RM_PLUGINS_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

G_BEGIN_DECLS

typedef struct _RmPlugin RmPlugin;

/**
 * RM_PLUGIN:
 * @NAME: prefix name of plugin functions
 *
 * Generic init function for plugins
 */
#define RM_PLUGIN(NAME) \
	G_MODULE_EXPORT void __rm_init_plugin(RmPlugin * plugin) { \
		plugin->init = NAME ## _plugin_init; \
		plugin->shutdown = NAME ## _plugin_shutdown; \
	}

/**
 * RM_PLUGIN_CONFIG:
 * @NAME: prefix name of plugin functions
 *
 * Generic init function with configuration for plugins
 */
#define RM_PLUGIN_CONFIG(NAME) \
	G_MODULE_EXPORT void __rm_init_plugin(RmPlugin * plugin) { \
		plugin->init = NAME ## _plugin_init; \
		plugin->shutdown = NAME ## _plugin_shutdown; \
		plugin->configure = NAME ## _plugin_configure; \
	}

/**
 * RmInitPlugin:
 * @plugin: a #RmPlugin
 *
 * Initializes a plugin
 *
 * Returns: %TRUE on success
 */
typedef gboolean (*RmInitPlugin)(RmPlugin *plugin);

/**
 * RmShutdownPlugin:
 * @plugin: a #RmPlugin
 *
 * Shutdowns a plugin
 *
 * Returns: %TRUE on success
 */
typedef gboolean (*RmShutdownPlugin)(RmPlugin *plugin);

/**
 * RmConfigurePlugin:
 * @plugin: a #RmPlugin
 *
 * Creates configuration view of a plugin
 *
 * Returns: pointer to a configuration widget
 */
typedef gpointer (*RmConfigurePlugin)(RmPlugin *plugin);

/**
 * RmPlugin:
 *
 * The #RmPlugin-struct contains only private fileds and should not be directly accessed.
 */
struct _RmPlugin {
	/*< private >*/
	gchar *name;
	gchar *description;
	gchar *copyright;
	RmInitPlugin init;
	RmShutdownPlugin shutdown;
	RmConfigurePlugin configure;

	gchar *module_name;
	gchar *help;
	gchar *homepage;
	gboolean builtin;
	gboolean enabled;
	gpointer priv;
	GModule *module;
};

void rm_plugins_init(void);
void rm_plugins_shutdown(void);
void rm_plugins_bind_loaded_plugins(void);

void rm_plugins_add_search_path(gchar *path);

GSList *rm_plugins_get(void);
void rm_plugins_disable(RmPlugin *plugin);
void rm_plugins_enable(RmPlugin *plugin);

G_END_DECLS

#endif
