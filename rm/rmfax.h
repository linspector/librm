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

#ifndef __RM_FAX_H__
#define __RM_FAX_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <rm/rmconnection.h>
#include <rm/rmdevice.h>

G_BEGIN_DECLS

/**
 * RmFaxPhase:
 * @RM_FAX_PHASE_CALL: Calling
 * @RM_FAX_PHASE_IDENTIFY: Identify remote fax
 * @RM_FAX_PHASE_SIGNALLING: Signalling data
 * @RM_FAX_PHASE_RELEASE: Releasing
 *
 * Phase of a fax connection
 */
typedef enum {
	RM_FAX_PHASE_CALL,
	RM_FAX_PHASE_IDENTIFY,
	RM_FAX_PHASE_SIGNALLING,
	RM_FAX_PHASE_RELEASE,
} RmFaxPhase;

/**
 * RmFaxStatus:
 *
 * The #RmFaxStatus-struct contains only private fileds and should not be directly accessed.
 */
typedef struct {
	/*< private >*/
	RmFaxPhase phase;
	gdouble percentage;
	gchar *remote_ident;
	gchar *local_ident;
	gchar *remote_number;
	gchar *local_number;
	gint bitrate;
	gint page_current;
	gint page_total;
	gint error_code;
} RmFaxStatus;

/**
 * RmFax:
 *
 * The #RmFax-struct contains only private fileds and should not be directly accessed.
 */
typedef struct {
	/*< private >*/
	RmDevice *device;
	gchar *name;
	RmConnection *(*send)(gchar * tiff, const gchar * target, gboolean anonymous);
	gboolean (*get_status)(RmConnection *connection, RmFaxStatus *fax_status);
	gint (*pickup)(RmConnection *connection);
	void (*hangup)(RmConnection *connection);

	gboolean (*number_is_handled)(gchar *number);
} RmFax;

void rm_fax_register(RmFax *fax);
GSList *rm_fax_get_plugins(void);
gchar *rm_fax_get_name(RmFax *fax);
gboolean rm_fax_get_status(RmFax *fax, RmConnection *connection, RmFaxStatus *status);
RmConnection *rm_fax_send(RmFax *fax, gchar *file, const gchar *target, gboolean anonymous);
RmFax *rm_fax_get(gchar *name);
void rm_fax_hangup(RmFax *fax, RmConnection *connection);

G_END_DECLS

#endif
