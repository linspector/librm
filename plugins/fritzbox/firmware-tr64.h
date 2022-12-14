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

#ifndef FIRMWARE_TR64_H
#define FIRMWARE_TR64_H

#include <rm/rm.h>

G_BEGIN_DECLS

gboolean firmware_tr64_is_available(RmProfile *profile);
GList *firmware_tr64_load_journal(RmProfile *profile);
gchar *firmware_tr64_load_voice(RmProfile *profile, const gchar *filename, gsize *len);
gboolean firmware_tr64_dial_number(RmProfile *profile, gint port, const gchar *number);
gboolean firmware_tr64_get_settings(RmProfile *profile);

G_END_DECLS

#endif
