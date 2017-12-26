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

#ifndef __RM_IMAGE_H__
#define __RM_IMAGE_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

G_BEGIN_DECLS

#define APP_ICON_ADD    "list-add-symbolic"
#define APP_ICON_REMOVE "list-remove-symbolic"
#define APP_ICON_TRASH  "user-trash-symbolic"
#define APP_ICON_CALL   "call-start-symbolic"
#define APP_ICON_HANGUP "call-stop-symbolic"

GdkPixbuf *rm_image_scale(GdkPixbuf *image, gint size);

G_END_DECLS

#endif
