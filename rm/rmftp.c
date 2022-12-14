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

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include <rm/rmrouter.h>
#include <rm/rmnetwork.h>
#include <rm/rmftp.h>

/**
 * SECTION:rmftp
 * @title: RmFtp
 * @short_description: Small ftp library
 * @stability: Stable
 *
 * Provides basic FTP functionality for interacting with the router.
 */

//#define FTP_DEBUG 1

/**
 * rm_ftp_open_port:
 * @server server name
 * @port port number
 *
 * Open FTP port channel
 *
 * Returns: io channel for given server:port
 */
static GIOChannel *rm_ftp_open_port(gchar *server, gint port)
{
	GSocket *socket = NULL;
	GInetAddress *inet_address = NULL;
	GSocketAddress *sock_address = NULL;
	GError *error = NULL;
	GResolver *resolver = NULL;
	GList *list = NULL;
	GList *tmp;
	GIOChannel *io_channel;
	gint sock;

	resolver = g_resolver_get_default();
	list = g_resolver_lookup_by_name(resolver, server, NULL, NULL);
	g_object_unref(resolver);

	if (list == NULL) {
		g_warning("Cannot resolve ip from hostname: %s!", server);
		return NULL;
	}

	/* We need a IPV4 connection */
	for (tmp = list; tmp != NULL; tmp = tmp->next) {
		if (g_inet_address_get_family(tmp->data) == G_SOCKET_FAMILY_IPV4) {
			inet_address = tmp->data;
		}
	}

	if (inet_address == NULL) {
		g_warning("Could not get ipv4 inet address from string: '%s'", server);
		g_object_unref(socket);
		g_resolver_free_addresses(list);
		return NULL;
	}

	sock_address = g_inet_socket_address_new(inet_address, port);
	if (sock_address == NULL) {
		g_warning("Could not create sock address on port %d", port);
		g_object_unref(socket);
		g_resolver_free_addresses(list);
		return NULL;
	}

	error = NULL;
	socket = g_socket_new(g_inet_address_get_family(inet_address), G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &error);

	error = NULL;
	if (g_socket_connect(socket, sock_address, NULL, &error) == FALSE) {
		g_warning("Could not connect to socket. Error: %s", error->message);
		g_error_free(error);
		g_object_unref(socket);
		g_resolver_free_addresses(list);
		return NULL;
	}

	sock = g_socket_get_fd(socket);

#ifdef G_OS_WIN32
	io_channel = g_io_channel_win32_new_socket(sock);
#else
	io_channel = g_io_channel_unix_new(sock);
#endif
	g_io_channel_set_encoding(io_channel, NULL, NULL);
	g_io_channel_set_buffered(io_channel, TRUE);

#ifdef FTP_DEBUG
	g_debug("%s(): connected on port %d", __FUNCTION__, port);
#endif

	return io_channel;
}

/**
 * rm_ftp_read_control_response:
 * @channel: open ftp io control channel
 * @len: pointer to store the data length to
 *
 * Read FTP control response from channel
 *
 * Returns: data response message or NULL on error
 */
gboolean rm_ftp_read_control_response(RmFtp *client)
{
	GError *error = NULL;
	GIOStatus io_status;
	gsize length;
	gchar match[5];
	gboolean multiline_start = FALSE;
	gboolean multiline_end = FALSE;

#ifdef FTP_DEBUG
	g_debug("Wait for control response");
#endif

	/* Clear previous response message */
	if (client->response) {
		g_free(client->response);
		client->response = NULL;
	}

	g_timer_start(client->timer);

	do {
		io_status = g_io_channel_read_line(client->control, &client->response, &length, NULL, &error);

		if (io_status == G_IO_STATUS_AGAIN) {
			g_usleep(500);
			continue;
		} else if (io_status != G_IO_STATUS_NORMAL) {
			g_warning("Got: io status %d, Error: %s", io_status, error->message);
			break;
		}

		if (!multiline_start) {
			client->code = g_ascii_strtoll(client->response, NULL, 10);
#ifdef FTP_DEBUG
			g_debug("Response: '%s'", client->response);
#endif
			if (client->response[3] == '-') {
				match[0] = client->response[0];
				match[1] = client->response[1];
				match[2] = client->response[2];
				match[3] = ' ';
				match[4] = '\0';

				multiline_start = TRUE;
			}
		} else {
#ifdef FTP_DEBUG
			g_debug("Response: '%s'", client->response);
#endif
			if (!strncmp(client->response, match, 4)) {
				multiline_end = TRUE;
			}
		}
	} while ((g_timer_elapsed(client->timer, NULL) < 5) && (!client->response || (multiline_start && !multiline_end)));

	g_timer_stop(client->timer);

	return client->response != NULL;
}

/**
 * rm_ftp_read_data_response:
 * @channel: open ftp io data channel
 * @len: pointer to store the data length to
 *
 * Read FTP data response from channel
 *
 * Returns: data response message or NULL on error
 */
gchar *rm_ftp_read_data_response(GIOChannel *channel, gsize *len)
{
	GError *error = NULL;
	GIOStatus io_status;
	gchar buffer[32768];
	gsize read;
	gchar *data;
	gsize data_size = 0;
	goffset data_offset = 0;

	data = NULL;

	while (1) {
		memset(buffer, 0, sizeof(buffer));
		io_status = g_io_channel_read_chars(channel, buffer, sizeof(buffer), &read, &error);

		if (io_status == G_IO_STATUS_NORMAL) {
			data_size += read;
			data = g_realloc(data, data_size + 1);
			memcpy(data + data_offset, buffer, read);
			data_offset += read;
			data[data_offset] = '\0';
		} else if (io_status == G_IO_STATUS_AGAIN) {
			continue;
		} else {
			break;
		}
	}

	if (len) {
		*len = data_offset;
	}

	return data;
}

/**
 * rm_ftp_send_command:
 * @client: a #RmFtp
 * @command: FTP command
 *
 * Send FTP command through io channel
 *
 * Returns: TRUE if data is available, FALSE on error
 */
gboolean rm_ftp_send_command(RmFtp *client, gchar *command)
{
	gchar *ftp_command;
	GIOStatus io_status;
	GError *error = NULL;
	gsize written;

	ftp_command = g_strconcat(command, "\r\n", NULL);

	io_status = g_io_channel_write_chars(client->control, ftp_command, strlen(ftp_command), &written, &error);
	g_free(ftp_command);
	if (io_status != G_IO_STATUS_NORMAL) {
		g_warning("Write error: %d", io_status);
		return FALSE;
	}
	g_io_channel_flush(client->control, NULL);

	return rm_ftp_read_control_response(client);
}

/**
 * rm_ftp_login:
 * @client: a #RmFtp
 * @user: username
 * @password: user password
 *
 * Login to FTP server
 *
 * Returns: %TRUE if login was successfull, otherwise %FALSE
 */
gboolean rm_ftp_login(RmFtp *client, const gchar *user, const gchar *password)
{
	gchar *cmd;
	gboolean login = FALSE;

	cmd = g_strconcat("USER ", user, NULL);
#ifdef FTP_DEBUG
	g_debug("ftp_login(): %s", cmd);
#endif
	rm_ftp_send_command(client, cmd);
	g_free(cmd);

	if (client->code == 331) {
		cmd = g_strconcat("PASS ", password, NULL);
#ifdef FTP_DEBUG
		g_debug("ftp_login(): PASS <xxxx>");
#endif
		rm_ftp_send_command(client, cmd);
		g_free(cmd);

		if (client->code == 230) {
			login = TRUE;
		}
	} else if (client->code == 230) {
		/* Already logged in */
		login = TRUE;
	}

	return login;
}

/**
 * rm_ftp_passive:
 * @client: a #RmFtp
 *
 * Switch FTP transfer to passive mode
 *
 * Returns: result, %TRUE for success, %FALSE on error
 */
gboolean rm_ftp_passive(RmFtp *client)
{
	gchar *pos;
	gint data_port;
	guint v[6];

#ifdef FTP_DEBUG
	g_debug("ftp_passive(): request");
#endif

	if (client->data) {
#ifdef FTP_DEBUG
		g_debug("Data channel already open");
#endif
		g_io_channel_shutdown(client->data, FALSE, NULL);
		g_io_channel_unref(client->data);
		client->data = NULL;
#ifdef FTP_DEBUG
		g_debug("ftp_passive(): data is NULL now");
#endif
	}

#ifdef FTP_DEBUG
	g_debug("ftp_passive(): EPSV");
#endif
	rm_ftp_send_command(client, "EPSV");

	if (client->code == 229) {
		pos = strchr(client->response, '|');
		if (!pos) {
			return FALSE;
		}

		pos += 3;
		sscanf(pos, "%u", &data_port);
	} else {
#ifdef FTP_DEBUG
		g_debug("ftp_passive(): PASV");
#endif
		rm_ftp_send_command(client, "PASV");

		if (client->code != 227) {
			return FALSE;
		}
		pos = strchr(client->response, '(');
		if (!pos) {
			return FALSE;
		}

		pos++;
		sscanf(pos, "%u,%u,%u,%u,%u,%u", &v[2], &v[3], &v[4], &v[5], &v[0], &v[1]);

#ifdef FTP_DEBUG
		//g_debug("ftp_passive(): v0 %d, v1: %d", v[0], v[1]);
#endif
		data_port = v[0] * 256 + v[1];
	}

#ifdef FTP_DEBUG
	g_debug("ftp_passive(): data_port: %d", data_port);
#endif

	client->data = rm_ftp_open_port(client->server, data_port);

	return client->data != NULL;
}

/**
 * rm_ftp_list_dir:
 * @client: a #RmFtp
 * @dir: directory name
 *
 * List FTP directory
 *
 * Returns: directory listing
 */
gchar *rm_ftp_list_dir(RmFtp *client, const gchar *dir)
{
	gchar *cmd = g_strconcat("CWD ", dir, NULL);
	gchar *response = NULL;

#ifdef FTP_DEBUG
	g_debug("ftp_list_dir(): %s", cmd);
#endif
	rm_ftp_send_command(client, cmd);
	g_free(cmd);

#ifdef FTP_DEBUG
	g_debug("ftp_list_dir(): NLST");
#endif
	rm_ftp_send_command(client, "NLST");

	if (client->code == 150) {
		response = rm_ftp_read_data_response(client->data, NULL);
	}

	return response;
}

/**
 * rm_ftp_get_file:
 * @client: a #RmFtp
 * @file: file to download
 * @len: pointer to store the file size to
 *
 * Get file of FTP
 *
 * Returns: file data or %NULL on error
 */
gchar *rm_ftp_get_file(RmFtp *client, const gchar *file, gsize *len)
{
	gchar *cmd = g_strconcat("RETR ", file, NULL);
	gchar *response = NULL;

	if (len) {
		*len = 0;
	}

#ifdef FTP_DEBUG
	g_debug("ftp_get_file(): TYPE I");
#endif
	rm_ftp_send_command(client, "TYPE I");

#ifdef FTP_DEBUG
	g_debug("ftp_get_file(): %s", cmd);
#endif
	rm_ftp_send_command(client, cmd);
	g_free(cmd);

	if (client->code == 150) {
		response = rm_ftp_read_data_response(client->data, len);
		rm_ftp_read_control_response(client);
	}

	return response;
}

/**
 * rm_ftp_put_file:
 * @client: a #RmFtp
 * @file: file to upload
 * @path: remote path
 * @data: file data
 * @size: file size
 *
 * Put file on FTP
 *
 * Returns: TRUE on success, otherwise FALSE
 */
gboolean rm_ftp_put_file(RmFtp *client, const gchar *file, const gchar *path, gchar *data, gsize size)
{
	gchar *cmd;
	gboolean passive;

#ifdef FTP_DEBUG
	g_debug("ftp_get_file(): TYPE I");
#endif
	rm_ftp_send_command(client, "TYPE I");
#ifdef FTP_DEBUG
	g_debug("ftp_put_file(): code=%d", client->code);
#endif
	if (client->code != 200) {
		return FALSE;
	}

	passive = rm_ftp_passive(client);
#ifdef FTP_DEBUG
	g_debug("ftp_put_file(): passive=%d", passive);
#endif
	if (!passive) {
		return FALSE;
	}

	cmd = g_strconcat("STOR ", path, "/", file, NULL);
#ifdef FTP_DEBUG
	g_debug("ftp_put_file(): %s", cmd);
#endif
	rm_ftp_send_command(client, cmd);
	g_free(cmd);
#ifdef FTP_DEBUG
	g_debug("ftp_put_file(): code=%d", client->code);
#endif
	if (client->code != 150) {
		return FALSE;
	}

	gsize written;
	GError *error = NULL;
#ifdef FTP_DEBUG
	g_debug("ftp_put_file(): write data");
#endif
	g_io_channel_set_buffer_size(client->data, 0);
	gsize chunk = g_io_channel_get_buffer_size(client->data);
	gsize offset = 0;
	gsize write;

	do {
		write = ((size - offset) > chunk) ? chunk : size - offset;
		GIOStatus status = g_io_channel_write_chars(client->data, data + offset, write, &written, &error);
		if (status == G_IO_STATUS_AGAIN) {
			continue;
		}
		if (status != G_IO_STATUS_NORMAL) {
			g_debug("ftp_put_file(): write failed.\n");
			return FALSE;
		}
		g_io_channel_flush(client->data, NULL);
		offset += written;
	} while (offset != size);
#ifdef FTP_DEBUG
	g_debug("ftp_put_file(): done");
#endif
	g_io_channel_shutdown(client->data, TRUE, &error);
	client->data = NULL;

	rm_ftp_read_control_response(client);

	return TRUE;
}

/**
 * rm_ftp_init:
 * @server: server host name
 *
 * Initialize ftp structure
 *
 * Returns: ftp structure or NULL on error
 */
RmFtp *rm_ftp_init(const gchar *server)
{
	RmFtp *client = g_slice_new0(RmFtp);

	client->server = g_strdup(server);
	client->control = rm_ftp_open_port(client->server, 21);
	if (!client->control) {
		g_warning("Could not connect to FTP-Port 21");
		g_free(client->server);
		g_slice_free(RmFtp, client);
		return NULL;
	}

	client->timer = g_timer_new();

	/* Read welcome message */
	rm_ftp_read_control_response(client);

#ifdef FTP_DEBUG
	g_debug("ftp_init() done");
#endif

	return client;
}

/**
 * rm_ftp_shutdown:
 * @client: a #RmFtp
 *
 * Shutdown ftp structure (close sockets and free memory)
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean rm_ftp_shutdown(RmFtp *client)
{
#ifdef FTP_DEBUG
	g_debug("ftp_shutdown(): start");
#endif

	g_return_val_if_fail(client != NULL, FALSE);

	g_timer_destroy(client->timer);

#ifdef FTP_DEBUG
	g_debug("ftp_shutdown(): free");
#endif
	g_free(client->server);
	g_free(client->response);

#ifdef FTP_DEBUG
	g_debug("ftp_shutdown(): shutdown");
#endif

	if (client->control) {
		g_io_channel_shutdown(client->control, FALSE, NULL);
		g_io_channel_unref(client->control);
	}

	if (client->data) {
		g_io_channel_shutdown(client->data, FALSE, NULL);
		g_io_channel_unref(client->data);
	}

#ifdef FTP_DEBUG
	g_debug("ftp_shutdown(): free");
#endif
	g_slice_free(RmFtp, client);

#ifdef FTP_DEBUG
	g_debug("ftp_shutdown(): done");
#endif

	return TRUE;
}

/**
 * rm_ftp_delete_file:
 * @client: a #RmFtp
 * @file: file to delete
 *
 * Delete file on FTP.
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean rm_ftp_delete_file(RmFtp *client, const gchar *file)
{
	gchar *cmd = g_strconcat("DELE ", file, NULL);

#ifdef FTP_DEBUG
	g_debug("ftp_delete_file(): %s", cmd);
#endif
	rm_ftp_send_command(client, cmd);
	g_free(cmd);

	if (client->code != 250) {
		g_debug("ftp_delete_file(): code: %d", client->code);
		return FALSE;
	}

	return TRUE;
}
