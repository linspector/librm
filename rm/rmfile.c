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

#include <gio/gio.h>

#include <rm/rmfile.h>

/**
 * SECTION:rmfile
 * @title: RmFile
 * @short_description: File loading and storing
 * @stability: Stable
 *
 * Offers convenient file loading and storing functionallity
 */

/**
 * rm_file_save:
 * @name: file name
 * @data: data pointer
 * @len: length of data
 *
 * Save @data of length @len to file @name.
 */
void rm_file_save(gchar *name, const gchar *data, gsize len)
{
	GFile *file = g_file_new_for_path(name);
	GError *error = NULL;
	GFileOutputStream *stream;

	if (!data) {
		g_warning("%s(): data is NULL", __FUNCTION__);
		return;
	}

	if (len == -1) {
		len = strlen(data);
	}

	if (g_file_query_exists(file, NULL)) {
		stream = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, &error);
	} else {
		gchar *dirname = g_path_get_dirname(name);
		g_mkdir_with_parents(dirname, 0700);
		g_free(dirname);

		stream = g_file_create(file, G_FILE_CREATE_PRIVATE, NULL, &error);
	}

	g_object_unref(file);

	if (stream != NULL) {
		g_output_stream_write(G_OUTPUT_STREAM(stream), data, len, NULL, NULL);
		g_output_stream_flush(G_OUTPUT_STREAM(stream), NULL, NULL);
		g_output_stream_close(G_OUTPUT_STREAM(stream), NULL, NULL);

		g_object_unref(stream);
	} else {
		g_warning("%s", error->message);
		g_error_free(error);
	}
}

/**
 * rm_file_load:
 * @name: file name
 * @size: pointer to store length of data to
 *
 * Load file @name.
 *
 * Returns: file data pointer
 */
gchar *rm_file_load(gchar *name, gsize *size)
{
	GFile *file;
	GFileInfo *file_info;
	goffset file_size;
	gchar *data = NULL;
	GFileInputStream *input_stream = NULL;

	file = g_file_new_for_path(name);
	if (!g_file_query_exists(file, NULL)) {
		g_object_unref(file);
		return NULL;
	}

	file_info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	file_size = g_file_info_get_size(file_info);
	if (file_size) {
		data = g_malloc0(file_size + 1);
		input_stream = g_file_read(file, NULL, NULL);

		g_input_stream_read_all(G_INPUT_STREAM(input_stream), data, file_size, size, NULL, NULL);

		g_object_unref(input_stream);
	}
	g_object_unref(file_info);
	g_object_unref(file);

	return data;
}
