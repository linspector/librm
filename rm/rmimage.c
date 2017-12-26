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

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <rm/rmimage.h>

/**
 * rm_image_scale:
 * @image: a #GdkPixbuf
 * @size: required size
 *
 * Scales the image using @size
 *
 * Returns: a new #GdxPixbuf
 */
GdkPixbuf *rm_image_scale(GdkPixbuf *image, gint size)
{
	g_assert(image != NULL);

	return gdk_pixbuf_scale_simple(image, size, size, GDK_INTERP_BILINEAR);
}
