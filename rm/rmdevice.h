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

#ifndef __RM_DEVICE_H__
#define __RM_DEVICE_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

G_BEGIN_DECLS

/**
 * RmDeviceType:
 * @RM_DEVICE_TYPE_PHONE: phone device
 * @RM_DEVICE_TYPE_FAX: fax device
 *
 * Type of device.
 */
typedef enum {
	RM_DEVICE_TYPE_PHONE,
	RM_DEVICE_TYPE_FAX
} RmDeviceType;

/**
 * RmDevice:
 *
 * The #RmDevice-struct contains only private fileds and should not be directly accessed.
 */
typedef struct _RmDevice {
	/*< private >*/
	gchar *name;
	gchar *settings_name;
} RmDevice;

/**
 * RmDeviceCast:
 *
 * The #RmDeviceCast-struct contains only private fileds and should not be directly accessed.
 */
typedef struct {
	/*< private >*/
	RmDevice *device;
	RmDeviceType type;

	gpointer priv;
} RmDeviceCast;

/**
 * RM_DEVICE:
 * @x: a @RmDeviceCast
 *
 * Cast a fax/phone device to plain device
 */
#define RM_DEVICE(x) (((RmDeviceCast*)(x))->device)

gboolean rm_device_handles_number(RmDevice *device, gchar *number);
void rm_device_set_numbers(RmDevice *device, gchar **numbers, const gchar *name);
gchar **rm_device_get_numbers(RmDevice *device);
RmDevice *rm_device_get(gchar *name);
gchar *rm_device_get_name(RmDevice *device);
RmDevice *rm_device_register(gchar *name);
void rm_device_unregister(RmDevice *device);
GSList *rm_device_get_plugins(void);

G_END_DECLS

#endif

