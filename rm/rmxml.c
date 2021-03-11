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

#include <glib.h>

#include <libxml/parser.h>

#include <rm/rmxml.h>

/**
 * SECTION:rmxml
 * @title: RmXml
 * @short_description: XML parsing functions
 * @stability: Stable
 *
 * Small subset of function for parsing and modifying XML files.
 */

/**
 * new_node:
 * @name: node name
 * @type: a #RmXmlNodeType
 *
 * Create new xml node
 *
 * Returns: new node pointer
 */
RmXmlNode *new_node(const gchar *name, RmXmlNodeType type)
{
	RmXmlNode *node = g_new0(RmXmlNode, 1);

	node->name = g_strdup(name);
	node->type = type;

	return node;
}

/**
 * rm_xmlnode_new:
 * @name: node name
 *
 * Create a new tag node
 *
 * Returns: a new #RmXmlNode or %NULL on error
 */
RmXmlNode *rm_xmlnode_new(const gchar *name)
{
	g_return_val_if_fail(name != NULL, NULL);

	return new_node(name, RM_XMLNODE_TYPE_TAG);
}

/**
 * rm_xmlnode_insert_child:
 * @parent: a #RmXmlNode
 * @child: a #RmXmlNode
 *
 * Insert child into parent node
 */
void rm_xmlnode_insert_child(RmXmlNode *parent, RmXmlNode *child)
{
	g_return_if_fail(parent != NULL);
	g_return_if_fail(child != NULL);

	child->parent = parent;

	if (parent->last_child) {
		parent->last_child->next = child;
	} else {
		parent->child = child;
	}

	parent->last_child = child;
}

/**
 * rm_xmlnode_new_child:
 * @parent: a #RmXmlNode
 * @name: node name
 *
 * Create new child node.
 *
 * Returns: new node pointer or %NULL on error
 */
RmXmlNode *rm_xmlnode_new_child(RmXmlNode *parent, const gchar *name)
{
	RmXmlNode *node;

	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	node = new_node(name, RM_XMLNODE_TYPE_TAG);

	rm_xmlnode_insert_child(parent, node);

	return node;
}

/**
 * rm_xmlnode_free:
 * @node: a #RmXmlNode
 *
 * Free node
 */
void rm_xmlnode_free(RmXmlNode *node)
{
	RmXmlNode *x, *y;

	g_return_if_fail(node != NULL);

	if (node->parent != NULL) {
		if (node->parent->child == node) {
			node->parent->child = node->next;
			if (node->parent->last_child == node) {
				node->parent->last_child = node->next;
			}
		} else {
			RmXmlNode *prev = node->parent->child;
			while (prev && prev->next != node) {
				prev = prev->next;
			}

			if (prev) {
				prev->next = node->next;
				if (node->parent->last_child == node) {
					node->parent->last_child = prev;
				}
			}
		}
	}

	x = node->child;
	while (x) {
		y = x->next;
		rm_xmlnode_free(x);
		x = y;
	}

	g_free(node->name);
	g_free(node->data);
	g_free(node->xml_ns);
	g_free(node->prefix);

	if (node->namespace_map) {
		g_hash_table_destroy(node->namespace_map);
	}

	g_free(node);
}

/**
 * Remove attribute from node
 * @node node pointer
 * @attr attribute name
 */
static void xmlnode_remove_attrib(RmXmlNode *node, const gchar *attr)
{
	RmXmlNode *attr_node, *sibling = NULL;

	g_return_if_fail(node != NULL);
	g_return_if_fail(attr != NULL);

	for (attr_node = node->child; attr_node != NULL; attr_node = attr_node->next) {
		if (attr_node->type == RM_XMLNODE_TYPE_ATTRIB && !strcmp(attr_node->name, attr)) {
			if (sibling == NULL) {
				node->child = attr_node->next;
			} else {
				sibling->next = attr_node->next;
			}

			if (node->last_child == attr_node) {
				node->last_child = sibling;
			}
			rm_xmlnode_free(attr_node);

			return;
		}
		sibling = attr_node;
	}
}

/**
 * rm_xmlnode_set_attrib:
 * @node: a #RmXmlNode
 * @attr: attribute name
 * @value: value to set
 *
 * Set attribute for node
 */
void rm_xmlnode_set_attrib(RmXmlNode *node, const gchar *attr, const gchar *value)
{
	RmXmlNode *attrib_node;

	g_return_if_fail(node != NULL);
	g_return_if_fail(attr != NULL);
	g_return_if_fail(value != NULL);

	xmlnode_remove_attrib(node, attr);

	attrib_node = new_node(attr, RM_XMLNODE_TYPE_ATTRIB);

	attrib_node->data = g_strdup(value);

	rm_xmlnode_insert_child(node, attrib_node);
}

/**
 * Get node prefix
 * @node node pointer
 * Returns: node prefix
 */
static const gchar *xmlnode_get_prefix(RmXmlNode *node)
{
	g_return_val_if_fail(node != NULL, NULL);

	return node->prefix;
}

/**
 * Convert node to string
 * @key key name
 * @value value
 * @buf buffer to print to
 */
static void xmlnode_to_str_foreach_append_ns(const gchar *key, const gchar *value, GString *buf)
{
	if (*key) {
		g_string_append_printf(buf, " xmlns:%s='%s'", key, value);
	} else {
		g_string_append_printf(buf, " xmlns='%s'", value);
	}
}

/**
 * Helps with converting node to string
 * @node node to convert
 * @len pointer for len saving
 * @formatting format text?
 * @depth depth
 * Returns: string data or NULL on error
 */
static gchar *xmlnode_to_str_helper(RmXmlNode *node, gint *len, gboolean formatting, gint depth)
{
	GString *text = g_string_new("");
	const gchar *prefix;
	RmXmlNode *c;
	gchar *node_name, *esc, *esc2, *tab = NULL;
	gboolean need_end = FALSE, pretty = formatting;

	g_return_val_if_fail(node != NULL, NULL);

	if (pretty && depth) {
		tab = g_strnfill(depth, '\t');
		text = g_string_append(text, tab);
	}

	node_name = g_markup_escape_text(node->name, -1);
	prefix = xmlnode_get_prefix(node);

	if (prefix) {
		g_string_append_printf(text, "<%s:%s", prefix, node_name);
	} else {
		g_string_append_printf(text, "<%s", node_name);
	}

	if (node->namespace_map) {
		g_hash_table_foreach(node->namespace_map, (GHFunc)xmlnode_to_str_foreach_append_ns, text);
	} else if (node->xml_ns) {
		if (!node->parent || !node->parent->xml_ns || strcmp(node->xml_ns, node->parent->xml_ns)) {
			gchar *xml_ns = g_markup_escape_text(node->xml_ns, -1);

			g_string_append_printf(text, " xmlns='%s'", xml_ns);
			g_free(xml_ns);
		}
	}

	for (c = node->child; c != NULL; c = c->next) {
		if (c->type == RM_XMLNODE_TYPE_ATTRIB) {
			const gchar *a_prefix = xmlnode_get_prefix(c);

			esc = g_markup_escape_text(c->name, -1);
			esc2 = g_markup_escape_text(c->data, -1);

			if (a_prefix) {
				g_string_append_printf(text, " %s:%s='%s'", a_prefix, esc, esc2);
			} else {
				g_string_append_printf(text, " %s='%s'", esc, esc2);
			}

			g_free(esc);
			g_free(esc2);
		} else if (c->type == RM_XMLNODE_TYPE_TAG || c->type == RM_XMLNODE_TYPE_DATA) {
			if (c->type == RM_XMLNODE_TYPE_DATA) {
				pretty = FALSE;
			}
			need_end = TRUE;
		}
	}

	if (need_end) {
		g_string_append_printf(text, ">%s", pretty ? "\n" : "");

		for (c = node->child; c != NULL; c = c->next) {
			if (c->type == RM_XMLNODE_TYPE_TAG) {
				gint esc_len;

				esc = xmlnode_to_str_helper(c, &esc_len, pretty, depth + 1);
				text = g_string_append_len(text, esc, esc_len);
				g_free(esc);
			} else if (c->type == RM_XMLNODE_TYPE_DATA && c->data_size > 0) {
				esc = g_markup_escape_text(c->data, c->data_size);
				text = g_string_append(text, esc);
				g_free(esc);
			}
		}

		if (tab && pretty) {
			text = g_string_append(text, tab);
		}
		if (prefix) {
			g_string_append_printf(text, "</%s:%s>%s", prefix, node_name, formatting ? "\n" : "");
		} else {
			g_string_append_printf(text, "</%s>%s", node_name, formatting ? "\n" : "");
		}
	} else {
		g_string_append_printf(text, "/>%s", formatting ? "\n" : "");
	}

	g_free(node_name);

	g_free(tab);

	if (len) {
		*len = text->len;
	}

	return g_string_free(text, FALSE);
}

/**
 * rm_xmlnode_to_formatted_str:
 * @node: a #RmXmlNode
 * @len: pointer to len
 *
 * Convet node to formatted string
 *
 * Returns: formatted string or %NULL on error
 */
gchar *rm_xmlnode_to_formatted_str(RmXmlNode *node, gint *len)
{
	gchar *xml, *xml_with_declaration;

	g_return_val_if_fail(node != NULL, NULL);

	xml = xmlnode_to_str_helper(node, len, TRUE, 0);
	xml_with_declaration = g_strdup_printf("<?xml version='1.0' encoding='UTF-8' ?>\n\n%s", xml);
	g_free(xml);

	if (len) {
		*len += sizeof("<?xml version='1.0' encoding='UTF-8' ?>\n\n") - 1;
	}

	return xml_with_declaration;
}

/** RmXmlNode parser data structure */
struct _xmlnode_parser_data {
	RmXmlNode *current;
	gboolean error;
};

/**
 * Set namespace
 * @node xml node
 * @xml_ns xml namespace
 */
void xmlnode_set_namespace(RmXmlNode *node, const gchar *xml_ns)
{
	g_return_if_fail(node != NULL);

	g_free(node->xml_ns);
	node->xml_ns = g_strdup(xml_ns);
}

/**
 * Set prefix
 * @node xml node
 * @prefix prefix
 */
static void xmlnode_set_prefix(RmXmlNode *node, const gchar *prefix)
{
	g_return_if_fail(node != NULL);

	g_free(node->prefix);
	node->prefix = g_strdup(prefix);
}

/**
 * rm_xmlnode_insert_data:
 * @node: a #RmXmlNode
 * @data: data pointer
 * @size: size of data
 *
 * Insert data into #RmXmlNode
 */
void rm_xmlnode_insert_data(RmXmlNode *node, const gchar *data, gssize size)
{
	RmXmlNode *child;
	gsize real_size;

	g_return_if_fail(node != NULL);
	g_return_if_fail(data != NULL);
	g_return_if_fail(size != 0);

	if (size == -1) {
		real_size = strlen(data);
	} else {
		real_size = size;
	}

	child = new_node(NULL, RM_XMLNODE_TYPE_DATA);

	child->data = g_memdup2(data, real_size);
	child->data_size = real_size;

	rm_xmlnode_insert_child(node, child);
}

/**
 * Set attribute with prefix
 * @node xml node
 * @attr attribute
 * @prefix prefix
 * @value value
 */
static void xmlnode_set_attrib_with_prefix(RmXmlNode *node, const gchar *attr, const gchar *prefix, const gchar *value)
{
	RmXmlNode *attrib_node;

	g_return_if_fail(node != NULL);
	g_return_if_fail(attr != NULL);
	g_return_if_fail(value != NULL);

	attrib_node = new_node(attr, RM_XMLNODE_TYPE_ATTRIB);

	attrib_node->data = g_strdup(value);
	attrib_node->prefix = g_strdup(prefix);

	rm_xmlnode_insert_child(node, attrib_node);
}

/**
 * rm_xmlnode_get_attrib:
 * @node: a #RmXmlNode
 * @attr: attribute name
 *
 * Get attribute from node
 *
 * Returns: attribute data
 */
const gchar *rm_xmlnode_get_attrib(RmXmlNode *node, const gchar *attr)
{
	RmXmlNode *x;

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(attr != NULL, NULL);

	for (x = node->child; x != NULL; x = x->next) {
		if (x->type == RM_XMLNODE_TYPE_ATTRIB && strcmp(attr, x->name) == 0) {
			return x->data;
		}
	}

	return NULL;
}

/**
 * Unescape html text
 * @html html text
 * Returns: unescaped text
 */
static gchar *unescape_html(const gchar *html)
{
	if (html != NULL) {
		const gchar *c = html;
		GString *ret = g_string_new("");

		while (*c) {
			if (!strncmp(c, "<br>", 4)) {
				ret = g_string_append_c(ret, '\n');
				c += 4;
			} else {
				ret = g_string_append_c(ret, *c);
				c++;
			}
		}

		return g_string_free(ret, FALSE);
	}

	return NULL;
}

/**
 * Parser: Element start
 * @user_data RmXmlNode parser data
 * @element_name element name
 * @prefix prefix
 * @xml_ns xml namespace
 * @nb_namespaces number of namespaces
 * @namespaces pointer to xml namespaces
 * @nb_attributes number of attributes
 * @nb_defaulted number of defaulted
 * @attributes pointer to xml attributes
 */
static void xmlnode_parser_element_start_libxml(void *user_data, const xmlChar *element_name, const xmlChar *prefix,
						const xmlChar *xml_ns, gint nb_namespaces, const xmlChar **namespaces, gint nb_attributes, gint nb_defaulted,
						const xmlChar **attributes)
{
	struct _xmlnode_parser_data *xpd = user_data;
	RmXmlNode *node;
	gint i, j;

	if (!element_name || xpd->error) {
		return;
	}

	if (xpd->current) {
		node = rm_xmlnode_new_child(xpd->current, (const gchar*)element_name);
	} else {
		node = rm_xmlnode_new((const gchar*)element_name);
	}

	xmlnode_set_namespace(node, (const gchar*)xml_ns);
	xmlnode_set_prefix(node, (const gchar*)prefix);

	if (nb_namespaces != 0) {
		node->namespace_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

		for (i = 0, j = 0; i < nb_namespaces; i++, j += 2) {
			const gchar *key = (const gchar*)namespaces[j];
			const gchar *val = (const gchar*)namespaces[j + 1];

			g_hash_table_insert(node->namespace_map, g_strdup(key ? key : ""), g_strdup(val ? val : ""));
		}
	}

	for (i = 0; i < nb_attributes * 5; i += 5) {
		const gchar *prefix = (const gchar*)attributes[i + 1];
		gchar *txt;
		gint attrib_len = attributes[i + 4] - attributes[i + 3];
		gchar *attrib = g_malloc(attrib_len + 1);

		memcpy(attrib, attributes[i + 3], attrib_len);
		attrib[attrib_len] = '\0';
		txt = attrib;
		attrib = unescape_html(txt);
		g_free(txt);

		if (prefix && *prefix) {
			xmlnode_set_attrib_with_prefix(node, (const gchar*)attributes[i], prefix, attrib);
		} else {
			rm_xmlnode_set_attrib(node, (const gchar*)attributes[i], attrib);
		}
		g_free(attrib);
	}

	xpd->current = node;
}

/**
 * Parser: Element end
 * @user_data RmXmlNode parser data
 * @element_name element name
 * @prefix prefix
 * @xml_ns xml namespace
 */
static void xmlnode_parser_element_end_libxml(void *user_data, const xmlChar *element_name, const xmlChar *prefix, const xmlChar *xml_ns)
{
	struct _xmlnode_parser_data *xpd = user_data;

	if (!element_name || !xpd->current || xpd->error) {
		return;
	}

	if (xpd->current->parent) {
		if (!xmlStrcmp((xmlChar*)xpd->current->name, element_name)) {
			xpd->current = xpd->current->parent;
		}
	}
}

/**
 * Parser: Element text
 * @user_data RmXmlNode parser data
 * @text text element
 * @text_len text length
 */
static void xmlnode_parser_element_text_libxml(void *user_data, const xmlChar *text, gint text_len)
{
	struct _xmlnode_parser_data *xpd = user_data;

	if (!xpd->current || xpd->error) {
		return;
	}

	if (!text || !text_len) {
		return;
	}

	rm_xmlnode_insert_data(xpd->current, (const gchar*)text, text_len);
}

/**
 * Parser error
 * @user_data RmXmlNode parser data
 * @msg error message
 */
static void xmlnode_parser_error_libxml(void *user_data, const gchar *msg, ...)
{
	struct _xmlnode_parser_data *xpd = user_data;
	gchar err_msg[2048];
	va_list args;

	xpd->error = TRUE;

	va_start(args, msg);
	vsnprintf(err_msg, sizeof(err_msg), msg, args);
	va_end(args);

	g_debug("Error parsing xml file: %s", err_msg);
}

/** RmXmlNode parser libxml */
static xmlSAXHandler xml_node_parser_libxml = {
	/* internalSubset */
	NULL,
	/* isStandalone */
	NULL,
	/* hasInternalSubset */
	NULL,
	/* hasExternalSubset */
	NULL,
	/* resolveEntity */
	NULL,
	/* getEntity */
	NULL,
	/* entityDecl */
	NULL,
	/* notationDecl */
	NULL,
	/* attributeDecl */
	NULL,
	/* elementDecl */
	NULL,
	/* unparsedEntityDecl */
	NULL,
	/* setDocumentLocator */
	NULL,
	/* startDocument */
	NULL,
	/* endDocument */
	NULL,
	/* startElement */
	NULL,
	/* endElement */
	NULL,
	/* reference */
	NULL,
	/* characters */
	xmlnode_parser_element_text_libxml,
	/* ignorableWhitespace */
	NULL,
	/* processingInstruction */
	NULL,
	/* comment */
	NULL,
	/* warning */
	NULL,
	/* error */
	xmlnode_parser_error_libxml,
	/* fatalError */
	NULL,
	/* getParameterEntity */
	NULL,
	/* cdataBlock */
	NULL,
	/* externalSubset */
	NULL,
	/* initialized */
	XML_SAX2_MAGIC,
	/* _private */
	NULL,
	/* startElementNs */
	xmlnode_parser_element_start_libxml,
	/* endElementNs */
	xmlnode_parser_element_end_libxml,
	/* serror */
	NULL,
};

/**
 * rm_xmlnode_from_str:
 * @str: string
 * @size: size of string
 *
 * Create RmXmlNode from string
 *
 * Returns: new #RmXmlNode
 */
RmXmlNode *rm_xmlnode_from_str(const gchar *str, gssize size)
{
	struct _xmlnode_parser_data *xpd;
	RmXmlNode *ret;
	gsize real_size;

	g_return_val_if_fail(str != NULL, NULL);

	real_size = size < 0 ? strlen(str) : size;
	xpd = g_new0(struct _xmlnode_parser_data, 1);

	if (xmlSAXUserParseMemory(&xml_node_parser_libxml, xpd, str, real_size) < 0) {
		while (xpd->current && xpd->current->parent) {
			xpd->current = xpd->current->parent;
		}

		if (xpd->current) {
			rm_xmlnode_free(xpd->current);
		}
		xpd->current = NULL;
	}
	ret = xpd->current;

	if (xpd->error) {
		ret = NULL;

		if (xpd->current) {
			rm_xmlnode_free(xpd->current);
		}
	}

	g_free(xpd);

	return ret;
}

/**
 * Get namespace
 * @node xml node
 * Returns: namespace
 */
const gchar *xmlnode_get_namespace(RmXmlNode *node)
{
	g_return_val_if_fail(node != NULL, NULL);

	return node->xml_ns;
}

/**
 * Get child with namespace
 * @parent parent xml node
 * @name child name
 * @ns namespace
 * Returns: chuld RmXmlNode
 */
RmXmlNode *xmlnode_get_child_with_namespace(const RmXmlNode *parent, const gchar *name, const gchar *ns)
{
	RmXmlNode *x, *ret = NULL;
	gchar **names;
	gchar *parent_name, *child_name;

	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	names = g_strsplit(name, "/", 2);
	parent_name = names[0];
	child_name = names[1];

	for (x = parent->child; x; x = x->next) {
		const gchar *xml_ns = NULL;

		if (ns != NULL) {
			xml_ns = xmlnode_get_namespace(x);
		}

		if (x->type == RM_XMLNODE_TYPE_TAG && name && !strcmp(parent_name, x->name) && (!ns || (xml_ns && !strcmp(ns, xml_ns)))) {
			ret = x;
			break;
		}
	}

	if (child_name && ret) {
		ret = rm_xmlnode_get_child(ret, child_name);
	}

	g_strfreev(names);

	return ret;
}

/**
 * rm_xmlnode_get_child:
 * @parent: xml node parent
 * @name: child name
 *
 * Get xml node child
 *
 * Returns: child #RmXmlNode
 */
RmXmlNode *rm_xmlnode_get_child(const RmXmlNode *parent, const gchar *name)
{
	return xmlnode_get_child_with_namespace(parent, name, NULL);
}

/**
 * rm_xmlnode_get_next_twin:
 * @node: xml node
 *
 * Get next twin from xml node
 *
 * Returns: next #RmXmlNode twin
 */
RmXmlNode *rm_xmlnode_get_next_twin(RmXmlNode *node)
{
	RmXmlNode *sibling;
	const gchar *ns = xmlnode_get_namespace(node);

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(node->type == RM_XMLNODE_TYPE_TAG, NULL);

	for (sibling = node->next; sibling; sibling = sibling->next) {
		const gchar *xml_ns = NULL;

		if (ns != NULL) {
			xml_ns = xmlnode_get_namespace(sibling);
		}

		if (sibling->type == RM_XMLNODE_TYPE_TAG && !strcmp(node->name, sibling->name) && (!ns || (xml_ns && !strcmp(ns, xml_ns)))) {
			return sibling;
		}
	}

	return NULL;
}

/**
 * rm_xmlnode_get_data:
 * @node: a #RmXmlNode
 *
 * Get data from RmXmlNode
 *
 * Returns: RmXmlNode data
 */
gchar *rm_xmlnode_get_data(RmXmlNode *node)
{
	GString *str = NULL;
	RmXmlNode *c;

	g_return_val_if_fail(node != NULL, NULL);

	for (c = node->child; c; c = c->next) {
		if (c->type == RM_XMLNODE_TYPE_DATA) {
			if (!str) {
				str = g_string_new_len(c->data, c->data_size);
			} else {
				str = g_string_append_len(str, c->data, c->data_size);
			}
		}
	}

	if (str == NULL) {
		return NULL;
	}

	return g_string_free(str, FALSE);
}

/**
 * Set data from RmXmlNode
 * @node xml node
 * @data data string
 * Returns: error code
 */
gint xmlnode_set_data(RmXmlNode *node, gchar *data)
{
	RmXmlNode *c;
	gint ret = -1;

	g_return_val_if_fail(node != NULL, ret);

	for (c = node->child; c != NULL; c = c->next) {
		if (c->type == RM_XMLNODE_TYPE_DATA) {
			if (c->data) {
				g_free(c->data);
				c->data = NULL;
			}

			c->data = g_strdup(data);
			c->data_size = strlen(c->data);
			ret = 0;
		}
	}

	return ret;
}

/**
 * rm_xml_read_from_file:
 * @file_name: xml file name
 *
 * Read xml from file.
 *
 * Returns: new #RmXmlNode or %NULL
 */
RmXmlNode *rm_xml_read_from_file(const gchar *file_name)
{
	const gchar *user_dir = g_get_user_data_dir();
	gchar *file_name_full = NULL;
	gchar *contents = NULL;
	gsize length;
	GError *error = NULL;
	RmXmlNode *node = NULL;

	g_return_val_if_fail(user_dir != NULL, NULL);

	file_name_full = g_build_filename(user_dir, file_name, NULL);
	if (!g_file_test(file_name_full, G_FILE_TEST_EXISTS)) {
		g_free(file_name_full);
		file_name_full = g_strdup(file_name);
		if (!g_file_test(file_name_full, G_FILE_TEST_EXISTS)) {
			g_free(file_name_full);
			return NULL;
		}
	}

	if (!g_file_get_contents(file_name_full, &contents, &length, &error)) {
		g_warning("'%s'", error->message);
		g_error_free(error);
	}

	if ((contents != NULL) && (length > 0)) {
		node = rm_xmlnode_from_str(contents, length);
		if (node == NULL) {
			g_warning("Could not parse node");
		}
		g_free(contents);
	}

	g_free(file_name_full);

	return node;
}

/**
 * Insert key/value into hash-table
 * @key key type
 * @value value type
 * @user_data pointer to hash table
 */
static void xmlnode_copy_foreach_ns(gpointer key, gpointer value, gpointer user_data)
{
	GHashTable *ret = (GHashTable*)user_data;
	g_hash_table_insert(ret, g_strdup(key), g_strdup(value));
}

/**
 * rm_xmlnode_copy:
 * @node: source #RmXmlNode
 *
 * Make a copy of a given node #RmXmlNode
 *
 * Returns: new #RmXmlNode
 */
RmXmlNode *rm_xmlnode_copy(const RmXmlNode *node)
{
	RmXmlNode *ret;
	RmXmlNode *child;
	RmXmlNode *sibling = NULL;

	g_return_val_if_fail(node != NULL, NULL);

	ret = new_node(node->name, node->type);
	ret->xml_ns = g_strdup(node->xml_ns);

	if (node->data) {
		if (node->data_size) {
			ret->data = g_memdup2(node->data, node->data_size);
			ret->data_size = node->data_size;
		} else {
			ret->data = g_strdup(node->data);
		}
	}

	ret->prefix = g_strdup(node->prefix);

	if (node->namespace_map) {
		ret->namespace_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_foreach(node->namespace_map, xmlnode_copy_foreach_ns, ret->namespace_map);
	}

	for (child = node->child; child; child = child->next) {
		if (sibling) {
			sibling->next = rm_xmlnode_copy(child);
			sibling = sibling->next;
		} else {
			ret->child = rm_xmlnode_copy(child);
			sibling = ret->child;
		}
		sibling->parent = ret;
	}

	ret->last_child = sibling;

	return ret;
}
