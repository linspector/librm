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

#ifndef CAPI_PHONE_H
#define CAPI_PHONE_H

#include <capi20.h>

#include <rm/rm.h>

#define PHONE_CIP 0x04

extern RmPhone capi_phone;

CapiConnection *capi_phone_call(guchar controller, const gchar *source, const gchar *target, gboolean anonymous);
void capi_phone_mute(RmConnection *connection, gboolean mute);
void capi_phone_hold(RmConnection *connection, gboolean mute);
void capi_phone_record(RmConnection *connection, gboolean record);
void capi_phone_send_dtmf_code(RmConnection *connection, guchar code);
void capi_phone_hangup(RmConnection *connection);
gint capi_phone_pickup(RmConnection *connection);
void capi_phone_data(CapiConnection *capi_connection, _cmsg message);
void capi_phone_conference(RmConnection *active, RmConnection *hold);
gint recording_write(struct recorder *recorder, short *buf, gint size, gint channel);

void capi_phone_init(RmDevice *device);
void capi_phone_shutdown(void);
void capi_phone_init_data(CapiConnection *connection);

#endif
