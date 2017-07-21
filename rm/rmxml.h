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

/*
 * \file xml.h
 * \brief XML node header
 */

#ifndef __RM_XML_H__
#define __RM_XML_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

G_BEGIN_DECLS

/**
 * RmXmlNodeType:
 *
 * The #RmXmlNodeType-struct contains only private fileds and should not be directly accessed.
 */
typedef enum {
	/*< private >*/
	RM_XMLNODE_TYPE_TAG,
	RM_XMLNODE_TYPE_ATTRIB,
	RM_XMLNODE_TYPE_DATA
} RmXmlNodeType;

/**
 * RmXmlNode:
 *
 * The #RmXmlNode-struct contains only private fileds and should not be directly accessed.
 */
typedef struct _RmXmlNode {
	/*< private >*/
	gchar *name;
	gchar *xml_ns;
	RmXmlNodeType type;
	gchar *data;
	size_t data_size;
	struct _RmXmlNode *parent;
	struct _RmXmlNode *child;
	struct _RmXmlNode *last_child;
	struct _RmXmlNode *next;
	gchar *prefix;
	GHashTable *namespace_map;
} RmXmlNode;

RmXmlNode *rm_xmlnode_new(const gchar *name);
RmXmlNode *rm_xmlnode_new_child(RmXmlNode *parent, const gchar *name);
RmXmlNode *rm_xml_read_from_file(const gchar *file_name);
RmXmlNode *rm_xmlnode_get_child(const RmXmlNode *parent, const gchar *name);
RmXmlNode *rm_xmlnode_get_next_twin(RmXmlNode *node);
gchar *rm_xmlnode_get_data(RmXmlNode *node);
const gchar *rm_xmlnode_get_attrib(RmXmlNode *node, const gchar *attr);
RmXmlNode *rm_xmlnode_from_str(const char *str, gssize size);
void rm_xmlnode_insert_data(RmXmlNode *node, const gchar *data, gssize size);
void rm_xmlnode_free(RmXmlNode *node);
void rm_xmlnode_set_attrib(RmXmlNode *node, const gchar *attr, const gchar *value);
void rm_xmlnode_insert_child(RmXmlNode *parent, RmXmlNode *child);
gchar *rm_xmlnode_to_formatted_str(RmXmlNode *node, gint *len);
RmXmlNode *rm_xmlnode_copy(const RmXmlNode *node);

G_END_DECLS

#endif
