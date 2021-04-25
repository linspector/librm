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

#include <tiff.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>

#include <capi.h>
#include <fax.h>
#include <isdn-convert.h>

#include <rm/rm.h>

static int8_t *_linear16_2_law = (int8_t*)&linear16_2_law[32768];
static uint16_t *_law_2_linear16 = &law_2_linear16[0];

/**
 * capi_fax_real_time_frame_handler:
 * @state: a #t30_state_t
 * @user_data: a #CapiFaxStatus
 * @direction: transmission direction
 * @msg: spandsp message
 * @len: length of frame
 *
 * Realtime frame handler which keeps tracks of data transfer
 */
static void capi_fax_real_time_frame_handler(t30_state_t *state, void *user_data, gint direction, const uint8_t *msg, gint len)
{
	CapiFaxStatus *status = user_data;
	t30_stats_t stats;

	if (msg[2] != 6) {
		return;
	}

	t30_get_transfer_statistics(state, &stats);

	if (status->sending) {
		status->bytes_total = stats.image_size;
		status->bytes_sent += len;
	} else {
		status->bytes_received += len;
		status->bytes_total += len;
	}
}

/**
 * capi_fax_phase_handler_b:
 * @state: a #t30_state_t
 * @user_data: a #CapiFaxStatus
 * @result: result
 *
 * Phase B handler
 *
 * Returns: 0
 */
static gint capi_fax_phase_handler_b(t30_state_t *state, void *user_data, gint result)
{
	CapiFaxStatus *status = user_data;
	t30_stats_t stats;
	t30_state_t *t30;
	const gchar *ident;

	t30_get_transfer_statistics(state, &stats);
	t30 = fax_get_t30_state(status->fax_state);

	ident = status->sending ? t30_get_rx_ident(t30) : t30_get_tx_ident(t30);
	snprintf(status->remote_ident, sizeof(status->remote_ident), "%s", ident ? ident : "");
	status->phase = PHASE_B;
	status->bytes_sent = 0;
	status->bytes_total = 0;
	status->bytes_received = 0;
	status->ecm = stats.error_correcting_mode;
	status->bitrate = stats.bit_rate;
	status->pages_transferred = status->sending ? stats.pages_tx : stats.pages_rx;

	g_debug("%s(): Phase B handler (0x%X) %s", __FUNCTION__, result, t30_frametype(result));
	g_debug("%s(): - bit rate %d", __FUNCTION__, stats.bit_rate);
	g_debug("%s(): - ecm %s", __FUNCTION__, (stats.error_correcting_mode) ? "on" : "off");
	g_debug("%s(): - Remote side: '%s'", __FUNCTION__, status->remote_ident);
	g_debug("%s(): - pages_transferred %d", __FUNCTION__, status->pages_transferred);

	return 0;
}

/**
 * capi_fax_phase_handler_d:
 * @state: a #t30_state_t
 * @user_data: a #CapiFaxStatus
 * @result: result
 *
 * Phase D handler
 *
 * Returns: 0
 */
static gint capi_fax_phase_handler_d(t30_state_t *state, void *user_data, gint result)
{
	CapiFaxStatus *status = user_data;
	t30_stats_t stats;

	t30_get_transfer_statistics(state, &stats);

	status->phase = PHASE_D;
	status->pages_transferred = status->sending ? stats.pages_tx : stats.pages_rx;
	status->bytes_sent = 0;

	g_debug("%s(): Phase D handler (0x%X) %s", __FUNCTION__, result, t30_frametype(result));
	g_debug("%s(): - pages transferred %d/%d", __FUNCTION__, status->sending ? stats.pages_tx : stats.pages_rx, stats.pages_in_file);

	return 0;
}

/**
 * capi_fax_phase_handler_e:
 * @state: a #t30_state_t
 * @user_data: a #CapiFaxStatus
 * @result: result code
 *
 * Phase E handler
 */
static void capi_fax_phase_handler_e(t30_state_t *state, void *user_data, gint result)
{
	CapiFaxStatus *status = user_data;
	t30_stats_t stats;

	t30_get_transfer_statistics(state, &stats);

	status->phase = PHASE_E;
	status->error_code = result;

	g_debug("%s(): Phase E handler (0x%X) %s", __FUNCTION__, result, t30_completion_code_to_str(result));
}

/**
 * capi_fax_get_tiff_total_pages:
 * @file: filename
 *
 * Get total pages in TIFF file
 *
 * Returns: number of pages
 */
static int capi_fax_get_tiff_total_pages(const char *file)
{
	TIFF *tiff_file;
	int max;

	if ((tiff_file = TIFFOpen(file, "r")) == NULL) {
		return -1;
	}

	max = 0;
	while (TIFFSetDirectory(tiff_file, (tdir_t)max)) {
		max++;
	}

	TIFFClose(tiff_file);

	return max;
}

/**
 * capi_fax_spandsp_init:
 * @tiff_file: tiff file to transfer
 * @sending: sending flag
 * @modem: supported modem
 * @ecm: error correction mode flag
 * @lsi: lsi
 * @local_header_info: local header
 * @connection: a #CapiConnection
 *
 * Initialize spandsp
 */
static void capi_fax_spandsp_init(const gchar *tiff_file, gboolean sending, gchar modem, gchar ecm, const gchar *lsi, const gchar *local_header_info, CapiConnection *connection)
{
	t30_state_t *t30;
	gint supported_resolutions = 0;
	gint supported_image_sizes = 0;
	gint supported_modems = 0;
	CapiFaxStatus *status = connection->priv;

	status->fax_state = fax_init(NULL, sending);

	t30 = fax_get_t30_state(status->fax_state);

	/* Supported resolutions */
	supported_resolutions = 0;
	supported_resolutions |= T30_SUPPORT_STANDARD_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_FINE_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_SUPERFINE_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_R8_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_R16_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_300_300_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_400_400_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_600_600_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_1200_1200_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_300_600_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_400_800_RESOLUTION;
	supported_resolutions |= T30_SUPPORT_600_1200_RESOLUTION;

	/* Supported image sizes */
	supported_image_sizes = 0;
	supported_image_sizes |= T30_SUPPORT_215MM_WIDTH;
	supported_image_sizes |= T30_SUPPORT_255MM_WIDTH;
	supported_image_sizes |= T30_SUPPORT_303MM_WIDTH;
	supported_image_sizes |= T30_SUPPORT_UNLIMITED_LENGTH;
	supported_image_sizes |= T30_SUPPORT_A4_LENGTH;
	supported_image_sizes |= T30_SUPPORT_US_LETTER_LENGTH;
	supported_image_sizes |= T30_SUPPORT_US_LEGAL_LENGTH;

	/* Supported modems */
	supported_modems = 0;
	if (modem > 0) {
		supported_modems |= T30_SUPPORT_V27TER;

		if (modem > 1) {
			supported_modems |= T30_SUPPORT_V29;

			if (modem > 2) {
				supported_modems |= T30_SUPPORT_V17;
			}
		}
	}

	t30_set_supported_modems(t30, supported_modems);

	/* Error correction */
	if (ecm) {
		/* Supported compressions */
#if defined(SPANDSP_SUPPORT_T85)
		t30_set_supported_compressions(t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION | T30_SUPPORT_T85_COMPRESSION);
#else
		t30_set_supported_compressions(t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);
#endif

		t30_set_ecm_capability(t30, ecm);
	}

	t30_set_supported_t30_features(t30, T30_SUPPORT_IDENTIFICATION | T30_SUPPORT_SELECTIVE_POLLING | T30_SUPPORT_SUB_ADDRESSING);
	t30_set_supported_resolutions(t30, supported_resolutions);
	t30_set_supported_image_sizes(t30, supported_image_sizes);

	if (lsi) {
		t30_set_tx_ident(t30, lsi);
	}
	if (local_header_info) {
		t30_set_tx_page_header_info(t30, local_header_info);
	}

	if (sending) {
		t30_set_tx_file(t30, tiff_file, -1, -1);
		status->pages_total = capi_fax_get_tiff_total_pages(tiff_file);
	} else {
		t30_set_rx_file(t30, tiff_file, -1);
	}

	t30_set_phase_b_handler(t30, capi_fax_phase_handler_b, status);
	t30_set_phase_d_handler(t30, capi_fax_phase_handler_d, status);
	t30_set_phase_e_handler(t30, capi_fax_phase_handler_e, status);
	t30_set_real_time_frame_handler(t30, capi_fax_real_time_frame_handler, status);
}

/**
 * capi_fax_spandsp_tx:
 * @fax_state: a #fax_state_t
 * @buf: transfer buffer
 * @len: length of buffer
 *
 * TX direction
 *
 * Returns: error code
 */
static gint capi_fax_spandsp_tx(fax_state_t *fax_state, guchar *buf, gsize len)
{
	gshort buf_in[CAPI_PACKETS];
	guchar *alaw;
	gint err, i;

	memset(buf_in, 0, sizeof (buf_in));
	err = fax_tx(fax_state, buf_in, CAPI_PACKETS);
	alaw = buf;

	for (i = 0; i != len && i < CAPI_PACKETS; ++i, ++alaw) {
		*alaw = _linear16_2_law[(int16_t)buf_in[i]];
	}

	return err;
}

/**
 * capi_fax_spandsp_rx:
 * @fax_state: a #fax_state_t
 * @buf: receive buffer
 * @len: length of buffer
 *
 * Process rx data through spandsp
 *
 * Returns: error code of fax_rx()
 */
static gint capi_fax_spandsp_rx(fax_state_t *fax_state, guchar *buf, gsize len)
{
	gshort buf_in[CAPI_PACKETS];
	gshort *wave;
	gint i;

	wave = buf_in;

	for (i = 0; i != len; ++i, ++wave) {
		*wave = _law_2_linear16[(uint8_t)buf[i]];
	}

	return fax_rx(fax_state, buf_in, CAPI_PACKETS);
}

/**
 * capi_fax_data:
 * @connection: a #CapiConnection
 * @capi_message: a #_cmsg
 *
 * Receive fax state
 */
void capi_fax_data(CapiConnection *connection, _cmsg capi_message)
{
	CapiFaxStatus *status = connection->priv;

	capi_fax_spandsp_rx(status->fax_state, DATA_B3_IND_DATA(&capi_message), DATA_B3_IND_DATALENGTH(&capi_message));
}

/**
 * capi_fax_tx_thread:
 * @data: a #CapiConnection
 *
 * Transfer fax
 *
 * Returns: %NULL
 */
static gpointer capi_fax_tx_thread(gpointer data)
{
	struct session *session = capi_get_session();
	CapiConnection *connection = data;
	CapiFaxStatus *status = connection->priv;

	while (connection->state == STATE_CONNECTED) {
		if (connection->use_buffers && connection->buffers < CAPI_BUFFERCNT) {
			guint8 alaw_buffer_tx[CAPI_PACKETS];
			gint32 len;

			/* Send data to remote */
			len = capi_fax_spandsp_tx(status->fax_state, alaw_buffer_tx, CAPI_PACKETS);

			if (len) {
				_cmsg cmsg;

				isdn_lock();
				DATA_B3_REQ(&cmsg, session->appl_id, 0, connection->ncci, (void*)alaw_buffer_tx, len, session->message_number++, 0);
				isdn_unlock();
				connection->buffers++;
			}
		} else {
			g_usleep(10);
		}
	}

	return NULL;
}

/**
 * capi_fax_init_data:
 * @connection: a #CapiConnection
 *
 * Initialize data transfer
 */
void capi_fax_init_data(CapiConnection *connection)
{
	g_thread_new("fax-tx-thread", capi_fax_tx_thread, connection);
}

/**
 * capi_fax_send:
 * @tiff_file: the Tiff file to send
 * @modem: 0-3 (2400-14400)
 * @ecm: Error correction mode (on/off)
 * @controller: The controller for sending the fax
 * @src_no: MSN
 * @trg_no: Target fax number
 * @lsi: Fax ident
 * @local_header_info: Fax header line
 * @call_anonymous: Send fax anonymous
 *
 * Send Fax
 *
 * Returns: a new #CapiConnection or %NULL on error
 */
CapiConnection *capi_fax_send(gchar *tiff_file, gint modem, gint ecm, gint controller, gint cip, const gchar *src_no, const gchar *trg_no, const gchar *lsi, const gchar *local_header_info, gboolean call_anonymous)
{
	CapiFaxStatus *status;
	CapiConnection *connection;

	g_debug("%s(): tiff: %s, modem: %d, ecm: %s, controller: %d, src: %s, trg: %s, ident: %s, header: %s, anonymous: %d)", __FUNCTION__, tiff_file, modem, ecm ? "on" : "off", controller, src_no, trg_no, (lsi != NULL ? lsi : "(null)"), (local_header_info != NULL ? local_header_info : "(null)"), call_anonymous);

	status = g_slice_new0(CapiFaxStatus);

	status->phase = IDLE;
	status->error_code = -1;
	status->sending = 1;
	status->manual_hookup = 0;
	status->modem = modem;
	status->ecm = ecm;
	snprintf(status->header, sizeof(status->header), "%s", local_header_info);
	snprintf(status->ident, sizeof(status->ident), "%s", lsi);
	snprintf(status->src_no, sizeof(status->src_no), "%s", src_no);
	snprintf(status->trg_no, sizeof(status->trg_no), "%s", trg_no);
	snprintf(status->tiff_file, sizeof(status->tiff_file), "%s", tiff_file);

	connection = capi_call(controller, src_no, trg_no, (guint)call_anonymous, SESSION_FAX, cip, 1, 1, 0, NULL, NULL, NULL);
	if (connection) {
		connection->priv = status;
		connection->buffers = 0;
		connection->use_buffers = TRUE;

		capi_fax_spandsp_init(status->tiff_file, TRUE, status->modem, status->ecm, status->ident, status->header, connection);
	}

	return connection;
}

/**
 * capi_fax_receive:
 * @tiff_file: The Tiff file for saving the fax
 * @modem: 0-3 (2400-14400)
 * @ecm: Error correction mode (on/off)
 * @src_no: MSN
 * @trg_no: After receiving a fax, dst_no is the senders fax number
 * @manual_hookup: Hook up manually
 *
 * Receive Fax
 *
 * Returns: error code
 */
gint capi_fax_receive(CapiConnection *connection, const gchar *tiff_file, gint modem, gint ecm, const gchar *src_no, gchar *trg_no, gboolean manual_hookup)
{
	CapiFaxStatus *status = NULL;
	gint ret = -2;

	g_debug("%s(): tiff: %s, modem: %d, ecm: %s, src: %s, manual: %s)", __FUNCTION__, tiff_file, modem, ecm ? "on" : "off", src_no, manual_hookup ? "on" : "off");

	if (!connection) {
		return ret;
	}

	status = g_slice_new0(CapiFaxStatus);

	status->phase = IDLE;
	status->sending = 0;
	status->modem = modem;
	status->ecm = ecm;
	status->manual_hookup = manual_hookup;
	status->error_code = -1;

	snprintf(status->src_no, sizeof(status->src_no), "%s", src_no);
	snprintf(status->tiff_file, sizeof(status->tiff_file), "%s", tiff_file);
	snprintf(trg_no, sizeof(status->trg_no), "%s", status->trg_no);

	connection->priv = status;

	capi_fax_spandsp_init(status->tiff_file, FALSE, status->modem, status->ecm, status->ident, status->header, connection);

	return 0;
}

/**
 * capi_fax_clean:
 * @connection: a #CapiConnection
 *
 * Cleanup private fax structure from capi connection
 */
void capi_fax_clean(CapiConnection *connection)
{
	CapiFaxStatus *status = connection->priv;

	g_debug("%s(): called", __FUNCTION__);

	if (status->fax_state != NULL) {
		fax_release(status->fax_state);
	}

	g_slice_free(CapiFaxStatus, status);
	connection->priv = NULL;
}

/**
 * capi_fax_dial:
 * @tiff: tiff file name
 * @trg_no: target number
 * @suppress: suppress number flag
 *
 * Dial number via fax
 *
 * Returns: a #RmConnection
 */
RmConnection *capi_fax_dial(gchar *tiff, const gchar *trg_no, gboolean suppress)
{
	RmProfile *profile = rm_profile_get_active();
	gint modem = g_settings_get_int(profile->settings, "fax-bitrate");
	gboolean ecm = g_settings_get_boolean(profile->settings, "fax-ecm");
	gint controller = g_settings_get_int(profile->settings, "fax-controller") + 1;
	gint cip = g_settings_get_int(profile->settings, "fax-cip");
	const gchar *src_no = g_settings_get_string(profile->settings, "fax-number");
	const gchar *header = g_settings_get_string(profile->settings, "fax-header");
	const gchar *ident = g_settings_get_string(profile->settings, "fax-ident");
	CapiConnection *capi_connection = NULL;
	RmConnection *connection = NULL;
	gchar *target;

	if (RM_EMPTY_STRING(src_no)) {
		rm_object_emit_message("Dial error", "Source MSN not set, cannot dial");
		return NULL;
	}

	target = rm_number_canonize(trg_no);

	if (cip == 1) {
		cip = FAX_CIP;
		g_debug("%s(): Using 'ISDN Fax' id", __FUNCTION__);
	} else {
		cip = SPEECH_CIP;
		g_debug("%s(): Using 'Analog Fax' id", __FUNCTION__);
	}

	capi_connection = capi_fax_send(tiff, modem, ecm, controller, cip, src_no, target, ident, header, suppress);
	g_free(target);

	if (capi_connection) {
		connection = rm_connection_add(&capi_fax, capi_connection->id, RM_CONNECTION_TYPE_OUTGOING, src_no, trg_no);
		connection->priv = capi_connection;
	}

	return connection;
}

/**
 * capi_fax_get_status:
 * @connection: a #RmConnection
 * @status: a #RmFaxStatus
 *
 * Get active fax status
 *
 * Returns: %TRUE
 */
gboolean capi_fax_get_status(RmConnection *connection, RmFaxStatus *status)
{
	CapiConnection *capi_connection = connection->priv;
	CapiFaxStatus *fax_status = capi_connection->priv;

	if (!fax_status) {
		return TRUE;
	}

	switch (fax_status->phase) {
	case PHASE_B:
		status->phase = RM_FAX_PHASE_IDENTIFY;
		break;
	case PHASE_D:
		status->phase = RM_FAX_PHASE_SIGNALLING;
		break;
	case PHASE_E:
		status->phase = RM_FAX_PHASE_RELEASE;
		break;
	default:
		status->phase = RM_FAX_PHASE_CALL;
		break;
	}

	status->remote_ident = rm_convert_utf8(fax_status->remote_ident, -1);
	status->pages_transferred = fax_status->pages_transferred;
	status->pages_total = fax_status->pages_total;
	status->error_code = fax_status->error_code;

	status->remote_number = g_strdup(fax_status->trg_no);
	status->local_ident = rm_convert_utf8(fax_status->header, -1);
	status->local_number = g_strdup(fax_status->src_no);
	status->bitrate = fax_status->bitrate;

	/* Percentage of previous pages */
	status->percentage = fax_status->pages_transferred;
	status->percentage /= fax_status->pages_total;

	/* Percentage of current page */
	status->percentage += ((float)fax_status->bytes_sent / (float)fax_status->bytes_total) / fax_status->pages_total;

	if (isnan(status->percentage)) {
		status->percentage = 0.0f;
	} else if (status->percentage > 1.0f) {
		status->percentage = 1.0f;
	}

	return TRUE;
}

/**
 * capi_fax_hangup:
 * @connection: a #RmConnection
 *
 * Hangup active fax connection
 */
static void capi_fax_hangup(RmConnection *connection)
{
	CapiConnection *capi_connection = connection->priv;

	capi_hangup(capi_connection);
}

RmFax capi_fax = {
	NULL,
	"CAPI Fax",
	capi_fax_dial,
	capi_fax_get_status,
	NULL,
	capi_fax_hangup,
};

/**
 * capi_fax_init:
 * @device: a #RmDevice
 *
 * Initialize capi fax
 */
void capi_fax_init(RmDevice *device)
{
	g_debug("%s(): called", __FUNCTION__);

	capi_fax.device = device;
	rm_fax_register(&capi_fax);
}
