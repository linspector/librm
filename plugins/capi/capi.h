/**
 * The rm project
 * Copyright (c) 2012-2014 Jan-Michael Brummer
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

/**
 * \file capi.h
 * \brief capi main header
 */

#ifndef CAPI_H
#define CAPI_H

/* CAPI headers */
#include <capi20.h>

#ifdef __linux__
#include <linux/capi.h>
#else

typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef struct capi_profile {
	uint16_t ncontroller;
	uint16_t nbchannel;
	uint32_t goptions;
	uint32_t support1;
	uint32_t support2;
	uint32_t support3;
	uint32_t reserved[6];
	uint32_t manu[5];
} capi_profile;
#endif

/* GLIB */
#include <glib.h>

/* C */
#include <ctype.h>
#include <unistd.h>

#include <sndfile.h>

#include <rm/rm.h>

#define CAPI_CONNECTIONS 5
/* Packet size */
#define CAPI_PACKETS 2048
/* Packet buffer count */
#define CAPI_BUFFERCNT 7
/* max. B-Channels */
#define CAPI_BCHANNELS 2

#define USE_ISDN_MUTEX 1

#ifdef USE_ISDN_MUTEX
#define isdn_lock() do { g_mutex_lock(&session->isdn_mutex); } while (0);
#define isdn_unlock() do { g_mutex_unlock(&session->isdn_mutex); } while (0);
#else
#define isdn_lock()
#define isdn_unlock()
#endif

enum state {
	STATE_IDLE = 0,
	STATE_CONNECT_REQ,
	STATE_CONNECT_WAIT,
	STATE_CONNECT_ACTIVE,
	STATE_CONNECT_B3_WAIT,
	STATE_CONNECTED,
	STATE_DISCONNECT_B3_REQ,
	STATE_DISCONNECT_B3_WAIT,
	STATE_DISCONNECT_ACTIVE,
	STATE_DISCONNET_WAIT,
	STATE_RINGING,
	STATE_INCOMING_WAIT,
	STATE_MAXSTATE
};

enum session_type {
	SESSION_NONE,
	SESSION_FAX,
	SESSION_PHONE
};

#define RECORDING_BUFSIZE 32768
#define RECORDING_JITTER 200

enum recording {
	RECORDING_LOCAL,
	RECORDING_REMOTE
};

struct record_channel {
	gint64 position;
	short buffer[RECORDING_BUFSIZE];
};

struct recorder {
	SNDFILE *file;
	char *file_name;

	gint64 start_time;
	struct record_channel local;
	struct record_channel remote;
	gint64 last_write;
};

struct capi_connection {
	enum state state;
	enum session_type type;

	guint id;
	guint controller;
	gulong plci;
	gulong ncci;
	gchar *ncpi;
	guint reason;
	guint reason_b3;
	gchar *source;
	gchar *target;
	void *priv;
	gint early_b3;
	gint hold;
	time_t connect_time;
	gboolean mute;
	gboolean recording;
	gdouble line_level_in_state;
	gdouble line_level_out_state;
	struct recorder recorder;
	gint buffers;
	gboolean use_buffers;

	gpointer audio;

	void (*init_data)(struct capi_connection *connection);
	void (*data)(struct capi_connection *connection, _cmsg capi_message);
	void (*clean)(struct capi_connection *connection);
};

typedef struct capi_connection CapiConnection;

typedef struct session {
	GMutex isdn_mutex;

	struct capi_connection connection[CAPI_CONNECTIONS];
	int appl_id;
	int message_number;
	int input_thread_state;
} CapiSession;

extern RmDevice *capi_device;

struct capi_connection *capi_get_free_connection(void);
struct capi_connection *capi_call(unsigned, const char *, const char *, unsigned, unsigned, unsigned, _cword, _cword, _cword, _cstruct, _cstruct, _cstruct);
void capi_send_dtmf_code(struct capi_connection *connection, unsigned char nCode);
void capi_hangup(struct capi_connection *connection);
int capi_pickup(struct capi_connection *connection, int type);

struct session *capi_get_session(void);
struct session *capi_session_init(const char *host, gint controller);
int capi_session_close(int force);

RmDevice *capi_get_device(void);

#endif
