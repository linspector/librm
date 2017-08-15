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

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <glib.h>

#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include <rm/rm.h>

typedef struct _pulse_pipes {
	pa_simple *simple_in;
	pa_simple *simple_out;
} PulseAudioPipes;

/** predefined backup values */
static gint pulse_channels = 1;
static gint pulse_sample_rate = 8000;
static gint pulse_bits_per_sample = 16;

typedef struct _pulse_device_list {
	gchar initialized;
	gchar name[512];
	gint index;
	gchar description[256];
} PulseAudioDeviceList;

/** This is where we'll store the input device list */
PulseAudioDeviceList input_device_list[16];
/** This is where we'll store the output device list */
PulseAudioDeviceList output_device_list[16];

/**
 * pulseaudio_state_cb:
 * @context: a #pa_context
 * @user_data: pa ready flag
 *
 * This callback gets called when our context changes state.
 */
static void pulseaudio_state_cb(pa_context *context, void *user_data)
{
	pa_context_state_t state;
	int *pulse_ready = user_data;

	state = pa_context_get_state(context);

	switch (state) {
	/* There are just here for reference */
	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
	default:
		break;
	case PA_CONTEXT_FAILED:
	case PA_CONTEXT_TERMINATED:
		*pulse_ready = 2;
		break;
	case PA_CONTEXT_READY:
		*pulse_ready = 1;
		break;
	}
}

/**
 * pulseaudio_sink_list_cb:
 * @context: a #pa_context
 * @sink_info: a #pa_sink_info
 * @eol: end-of-list
 * @user_data: pointer to device list
 *
 * Mainloop will call this function when it's ready to tell us about a sink.
 */
static void pulseaudio_sink_list_cb(pa_context *context, const pa_sink_info *sink_info, int eol, void *user_data)
{
	PulseAudioDeviceList *device_list = user_data;
	int index = 0;

	/* If eol is set to a positive number, you're at the end of the list */
	if (eol > 0) {
		return;
	}

	for (index = 0; index < 16; index++) {
		if (!device_list[index].initialized) {
			strncpy(device_list[index].name, sink_info->name, 511);
			strncpy(device_list[index].description, sink_info->description, 255);
			device_list[index].index = sink_info->index;
			device_list[index].initialized = 1;
			break;
		}
	}
}

/**
 * pulseaudio_source_list_cb:
 * @context: a #pa_context
 * @source_info: a #pa_source_info
 * @eol: end-of-list
 * @user_data: pointer to device list
 *
 * See above.  This callback is pretty much identical to the previous
 */
static void pulseaudio_source_list_cb(pa_context *context, const pa_source_info *source_info, int eol, void *user_data)
{
	PulseAudioDeviceList *device_list = user_data;
	int index = 0;

	if (eol > 0) {
		return;
	}

	for (index = 0; index < 16; index++) {
		if (!device_list[index].initialized) {
			strncpy(device_list[index].name, source_info->name, 511);
			strncpy(device_list[index].description, source_info->description, 255);
			device_list[index].index = source_info->index;
			device_list[index].initialized = 1;
			break;
		}
	}
}

/**
 * pulseaudio_get_device_list:
 * @input: pointer input device list
 * @output: pointer output device list
 *
 * Get device list for input and output devices
 *
 * Returns: error code
 */
static int pulseaudio_get_device_list(PulseAudioDeviceList *input, PulseAudioDeviceList *output)
{
	/* Define our pulse audio loop and connection variables */
	pa_mainloop *main_loop;
	pa_mainloop_api *main_api;
	pa_operation *operation = NULL;
	pa_context *context;
	/* We'll need these state variables to keep track of our requests */
	int state = 0;
	int ready = 0;

	/* Initialize our device lists */
	memset(input, 0, sizeof(PulseAudioDeviceList) * 16);
	memset(output, 0, sizeof(PulseAudioDeviceList) * 16);

	/* Create a mainloop API and connection to the default server */
	main_loop = pa_mainloop_new();
	main_api = pa_mainloop_get_api(main_loop);
	context = pa_context_new(main_api, "test");

	/* This function connects to the pulse server */
	pa_context_connect(context, NULL, 0, NULL);

	/**
	 * This function defines a callback so the server will tell us it's state.
	 * Our callback will wait for the state to be ready.  The callback will
	 * modify the variable to 1 so we know when we have a connection and it's
	 * ready.
	 * If there's an error, the callback will set pa_ready to 2
	 */
	pa_context_set_state_callback(context, pulseaudio_state_cb, &ready);

	/**
	 * Now we'll enter into an infinite loop until we get the data we receive
	 * or if there's an error
	 */
	for (;; ) {
		/**
		 * We can't do anything until PA is ready, so just iterate the mainloop
		 * and continue
		 */
		if (ready == 0) {
			pa_mainloop_iterate(main_loop, 1, NULL);
			continue;
		}

		/* We couldn't get a connection to the server, so exit out */
		if (ready == 2) {
			pa_context_disconnect(context);
			pa_context_unref(context);
			pa_mainloop_free(main_loop);
			return -1;
		}

		/**
		 * At this point, we're connected to the server and ready to make
		 * requests
		 */
		switch (state) {
		/* State 0: we haven't done anything yet */
		case 0:
			operation = pa_context_get_sink_info_list(context, pulseaudio_sink_list_cb, output);

			/* Update state for next iteration through the loop */
			state++;
			break;
		case 1:
			if (pa_operation_get_state(operation) == PA_OPERATION_DONE) {
				pa_operation_unref(operation);

				operation = pa_context_get_source_info_list(context, pulseaudio_source_list_cb, input);

				/* Update the state so we know what to do next */
				state++;
			}
			break;
		case 2:
			if (pa_operation_get_state(operation) == PA_OPERATION_DONE) {
				pa_operation_unref(operation);
				pa_context_disconnect(context);
				pa_context_unref(context);
				pa_mainloop_free(main_loop);
				return 0;
			}
			break;
		default:
			g_warning("%s(): in state %d", __FUNCTION__, state);
			return -1;
		}

		pa_mainloop_iterate(main_loop, 1, NULL);
	}
}

/**
 * pulseaudio_detect_devices:
 *
 * Detect pulseaudio devices
 *
 * Returns: list of audio devices
 */
GSList *pulseaudio_detect_devices(void)
{
	int found_in = 0;
	int found_out = 0;
	int index;
	GSList *list = NULL;
	RmAudioDevice *device;

	if (pulseaudio_get_device_list(input_device_list, output_device_list) < 0) {
		g_warning("%s(): failed to get device list", __FUNCTION__);
		return list;
	}

	for (index = 0; index < 16; index++) {
		if (!output_device_list[index].initialized || strstr(output_device_list[index].name, ".monitor")) {
			continue;
		}
		found_out++;

		device = g_slice_new0(RmAudioDevice);
		device->internal_name = g_strdup(output_device_list[index].name);
		device->name = g_strdup(output_device_list[index].description);
		device->type = RM_AUDIO_OUTPUT;
		list = g_slist_prepend(list, device);
	}

	for (index = 0; index < 16; index++) {
		if (!input_device_list[index].initialized || strstr(input_device_list[index].name, ".monitor")) {
			continue;
		}

		device = g_slice_new0(RmAudioDevice);
		device->internal_name = g_strdup(input_device_list[index].name);
		device->name = g_strdup(input_device_list[index].description);
		device->type = RM_AUDIO_INPUT;
		list = g_slist_prepend(list, device);

		found_in++;
	}

	return list;
}

/**
 * pulseaudio_init:
 * @channels: number of channels
 * @sample_rate: sample rate
 * @bits_per_sample: number of bits per samplerate
 *
 * Initialize audio device
 *
 * Returns: %TRUE on success, otherwise error
 */
static int pulseaudio_init(unsigned char channels, unsigned short sample_rate, unsigned char bits_per_sample)
{
	/* TODO: Check if configuration is valid and usable */
	pulse_channels = channels;
	pulse_sample_rate = sample_rate;
	pulse_bits_per_sample = bits_per_sample;

	return 0;
}

/**
 * pulseaudio_open:
 * @output: specific audio output device
 *
 * Open new playback and record pipes
 *
 * Returns: pipe pointer or %NULL on error
 */
static void *pulseaudio_open(gchar *output)
{
	pa_sample_spec sample_spec = {
		.format = PA_SAMPLE_S16LE,
	};
	int error;
	pa_buffer_attr buffer = {
		.fragsize = 320,
		.maxlength = -1,
		.minreq = -1,
		.prebuf = -1,
		.tlength = -1,
	};
	PulseAudioPipes *pipes = malloc(sizeof(PulseAudioPipes));
	const gchar *input;

	if (!pipes) {
		g_warning("%s(): Could not get memory for pipes", __FUNCTION__);
		return NULL;
	}

	sample_spec.rate = pulse_sample_rate;
	sample_spec.channels = pulse_channels;
	if (pulse_bits_per_sample == 2) {
		sample_spec.format = PA_SAMPLE_S16LE;
	}

	if (RM_EMPTY_STRING(output)) {
		output = NULL;
	}

	pipes->simple_out = pa_simple_new(NULL, "Router Manager", PA_STREAM_PLAYBACK, output, "phone", &sample_spec, NULL, NULL, &error);
	if (pipes->simple_out == NULL) {
		g_debug("%s(): Could not open output device '%s'. Error: %s", __FUNCTION__, output ? output : "", pa_strerror(error));
		free(pipes);
		return NULL;
	}

	input = g_settings_get_string(rm_profile_get_active()->settings, "audio-input");
	if (RM_EMPTY_STRING(input)) {
		input = NULL;
	}

	pipes->simple_in = pa_simple_new(NULL, "Router Manager", PA_STREAM_RECORD, input, "phone", &sample_spec, NULL, &buffer, &error);
	if (pipes->simple_in == NULL) {
		g_debug("%s(): Could not open input device '%s'. Error: %s", __FUNCTION__, input ? input : "", pa_strerror(error));
	}

	return pipes;
}

/**
 * pulseaudio_close:
 * @priv: pointer to private input/output pipes
 *
 * Close audio pipelines
 *
 * Returns: error code
 */
static int pulseaudio_close(void *priv)
{
	PulseAudioPipes *pipes = priv;

	if (!pipes) {
		return -EINVAL;
	}

	if (pipes->simple_out) {
		pa_simple_free(pipes->simple_out);
		pipes->simple_out = NULL;
	}

	if (pipes->simple_in) {
		pa_simple_free(pipes->simple_in);
		pipes->simple_in = NULL;
	}

	free(pipes);

	return 0;
}

/**
 * pulseaudio_write:
 * @priv: pointer to private pipes
 * @buf: audio data pointer
 * @len: length of buffer
 *
 * Write data to audio device
 *
 * Returns: error code
 */
static gsize pulseaudio_write(void *priv, guchar *buf, gsize len)
{
	PulseAudioPipes *pipes = priv;
	int error;

	if (pipes == NULL || pipes->simple_out == NULL) {
		return -1;
	}

	if (pa_simple_write(pipes->simple_out, buf, (size_t)len, &error) < 0) {
		g_debug("%s(): Failed: %s", __FUNCTION__, pa_strerror(error));
	}

	return 0;
}

/**
 * pulseaudio_read:
 * @priv: pointer to private pipes
 * @buf: audio data pointer
 * @len: maximal length of buffer
 *
 * Read data from audio device
 *
 * Returns: error code
 */
static gsize pulseaudio_read(void *priv, guchar *buf, gsize len)
{
	PulseAudioPipes *pipes = priv;
	int nRet = 0;
	int error;

	if (!pipes || !pipes->simple_in) {
		return 0;
	}

	nRet = pa_simple_read(pipes->simple_in, buf, len, &error);
	if (nRet < 0) {
		g_debug("%s(): Failed: %s", __FUNCTION__, pa_strerror(error));
		len = 0;
	}

	return len;
}

/**
 * pulseaudio_shutdown:
 *
 * Shutdown audio interface
 *
 * Returns: 0
 */
static int pulseaudio_shutdown(void)
{
	return 0;
}

/** audio definition */
RmAudio pulseaudio = {
	"PulseAudio",
	pulseaudio_init,
	pulseaudio_open,
	pulseaudio_write,
	pulseaudio_read,
	pulseaudio_close,
	pulseaudio_shutdown,
	pulseaudio_detect_devices
};

/**
 * pulseaudio_plugin_init:
 * @plugin: a #RmPlugin
 *
 * Activate plugin (add net event)
 *
 * Returns: %TRUE
 */
static gboolean pulseaudio_plugin_init(RmPlugin *plugin)
{
	/* Set media role */
	g_setenv("PULSE_PROP_media.role", "phone", TRUE);
	g_setenv("PULSE_PROP_filter.want", "echo-cancel", TRUE);

	rm_audio_register(&pulseaudio);

	return TRUE;
}

/**
 * pulseaudio_plugin_shutdown:
 * @plugin: a #RmPlugin
 *
 * Deactivate plugin (remote net event)
 *
 * Returns: %TRUE
 */
static gboolean pulseaudio_plugin_shutdown(RmPlugin *plugin)
{
	rm_audio_unregister(&pulseaudio);

	return TRUE;
}

RM_PLUGIN(pulseaudio);
