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

#ifndef FAX_H
#define FAX_H

#include <rm/rm.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>

#include <capi20.h>

/* Service indicator (0x04=speech, 0x11=fax/g3) */
#define SPEECH_CIP                      0x04
#define FAX_CIP                         0x11

enum fax_phase {
	IDLE = -1,
	CONNECT = 1,
	PHASE_B = 2,
	PHASE_D = 3,
	PHASE_E = 4,
};

typedef struct {
	gchar tiff_file[256];
	gchar src_no[64];
	gchar trg_no[64];
	gchar ident[64];
	gchar header[64];
	gchar remote_ident[64];

	enum fax_phase phase;
	gint error_code;
	gboolean sending;
	gchar ecm;
	gchar modem;
	gint bitrate;
	gint pages_transferred;
	gint pages_total;
	gint bytes_received;
	gint bytes_sent;
	gint bytes_total;
	gboolean manual_hookup;
	gboolean done;

	fax_state_t *fax_state;
} CapiFaxStatus;

extern RmFax capi_fax;

CapiConnection *capi_fax_send(gchar *tiff_file, gint modem, gint ecm, gint controller, gint cip, const gchar *src_no, const gchar *trg_no, const gchar *lsi, const gchar *local_header_info, gint call_anonymous);
gint capi_fax_recv(const gchar *tiff_file, gint modem, gint ecm, const gchar *src_no, gchar *trg_no, const gchar *lsi, const gchar *local_header_info, gint manual_hookup);
void capi_fax_data(CapiConnection *connection, _cmsg message);
void capi_fax_clean(CapiConnection *connection);
void capi_fax_init_data(CapiConnection *connection);
void capi_fax_init(RmDevice *device);

#endif
