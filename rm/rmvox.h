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

#ifndef __RM_VOX_H__
#define __RM_VOX_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <rm/rmaudio.h>

G_BEGIN_DECLS

typedef struct _RmVoxPlayback RmVoxPlayback;

RmVoxPlayback *rm_vox_init(gconstpointer data, gsize len, GError **error);
gboolean rm_vox_play(RmVoxPlayback *playback);
gboolean rm_vox_shutdown(RmVoxPlayback *playback);
gboolean rm_vox_set_pause(RmVoxPlayback *playback, gboolean state);
gboolean rm_vox_seek(RmVoxPlayback *playback, gdouble pos);
gint rm_vox_get_fraction(RmVoxPlayback *playback);
gfloat rm_vox_get_seconds(RmVoxPlayback *playback);
void rm_vox_use_ringtone_audio(RmVoxPlayback *playback, gboolean ringtone);

G_END_DECLS

#endif
