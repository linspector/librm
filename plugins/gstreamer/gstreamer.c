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

#include <stdlib.h>
#include <string.h>

#include <gst/base/gstadapter.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#include <rm/rm.h>

/** predefined backup values */
static gint gstreamer_channels = 2;
static gint gstreamer_sample_rate = 8000;
static gint gstreamer_bits_per_sample = 16;
static GstDeviceMonitor *monitor = NULL;
static gboolean use_gst_device_monitor = FALSE;

#define SAMPLE_SIZE 160

typedef struct _GstreamerPipes {
	GstElement *in_pipe;
	GstElement *out_pipe;
	GstElement *in_bin;
	GstElement *out_bin;
	GstAdapter *adapter;
} GstreamerPipes;

/**
 * gstreamer_pipeline_cleaner:
 * @bus: a #GstBus
 * @message: a #GstMessage
 * @pipeline: pipeline we want to stop
 *
 * Stop and clean gstreamer pipeline
 *
 * Returns: %TRUE if we received a EOS for the requested pipeline
 */
static gboolean gstreamer_pipeline_cleaner(GstBus *bus, GstMessage *message, gpointer pipeline)
{
	gboolean result = TRUE;

	/* If we receive a End-Of-Stream message for the requested pipeline, stop the pipeline */
	if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS && GST_MESSAGE_SRC(message) == GST_OBJECT_CAST(pipeline)) {
		result = FALSE;
		gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
		g_object_unref(pipeline);
	}

	return result;
}

/**
 * gstreamer_detect_devices:
 *
 * Detect supported audio devices.
 *
 * Returns: list of audio device (output/input)
 */
static GSList *gstreamer_detect_devices(void)
{
	GSList *devices = NULL;
	RmAudioDevice *audio_device;
	GList *gst_devices = NULL;

	if (!use_gst_device_monitor) {
		/* We do not use the gstreamer device monitor, fallback to auto devices */
		audio_device = g_slice_new0(RmAudioDevice);

		audio_device->internal_name = g_strdup("autoaudiosink");
		audio_device->name = g_strdup("Automatic");
		audio_device->type = RM_AUDIO_OUTPUT;
		devices = g_slist_prepend(devices, audio_device);

		audio_device = g_slice_new0(RmAudioDevice);
		audio_device->internal_name = g_strdup("autoaudiosrc");
		audio_device->name = g_strdup("Automatic");
		audio_device->type = RM_AUDIO_INPUT;
		devices = g_slist_prepend(devices, audio_device);

		return devices;
	}

	/* Get devices from monitor */
	gst_devices = gst_device_monitor_get_devices(monitor);
	GList *list;

	for (list = gst_devices; list != NULL; list = list->next) {
		GstDevice *device = list->data;
		gchar *name = gst_device_get_display_name(device);
		gchar *class;

		if (strstr(name, "Monitor")) {
			continue;
		}

		class = gst_device_get_device_class(device);

		if (!strcmp(class, "Audio/Sink")) {
			audio_device = g_slice_new0(RmAudioDevice);
			audio_device->internal_name = g_strdup(name);
			audio_device->name = g_strdup(name);
			audio_device->type = RM_AUDIO_OUTPUT;
			devices = g_slist_prepend(devices, audio_device);
		} else if (!strcmp(class, "Audio/Source")) {
			audio_device = g_slice_new0(RmAudioDevice);
			audio_device->internal_name = g_strdup(name);
			audio_device->name = g_strdup(name);
			audio_device->type = RM_AUDIO_INPUT;
			devices = g_slist_prepend(devices, audio_device);
		}
	}

	return devices;
}

/**
 * gstreamer_set_buffer_output_size:
 * @priv: a #GstElement pipeline
 * @buffer_size: requested buffer size
 *
 * Set buffer size we want to use.
 */
static void gstreamer_set_buffer_output_size(gpointer priv, unsigned buffer_size)
{
	GstElement *src = NULL;
	GstElement *pipeline = priv;

	/* Try to receive pipeline bin name */
	src = gst_bin_get_by_name(GST_BIN(pipeline), "rm_src");
	if (src != NULL) {
		/* set blocksize */
		g_object_set(G_OBJECT(src), "blocksize", buffer_size, NULL);
		g_object_unref(src);
	}
}

/**
 * gstreamer_set_buffer_input_size:
 * @priv: a #GstElement pipeline
 * @buffer_size: requested buffer size
 *
 * Set buffer size we want to use.
 */
void gstreamer_set_buffer_input_size(gpointer priv, unsigned buffer_size)
{
	GstElement *sink = NULL;
	GstElement *pipeline = priv;

	/* Try to receive pipeline bin name */
	sink = gst_bin_get_by_name(GST_BIN(pipeline), "rm_sink");
	if (sink != NULL) {
		/* set blocksize */
		g_object_set(G_OBJECT(sink), "blocksize", buffer_size, NULL);
		g_object_unref(sink);
	}
}

/**
 * gstreamer_init:
 * @channels: number of channels
 * @sample_rate: sample rate
 * @bits_per_sample: number of bits per samplerate
 *
 * Initialize audio device
 *
 * Returns: %TRUE on success, otherwise error
 */
static int gstreamer_init(unsigned char channels, unsigned short sample_rate, unsigned char bits_per_sample)
{
	monitor = gst_device_monitor_new();

	gst_device_monitor_add_filter(monitor, "Audio/Sink", NULL);
	gst_device_monitor_add_filter(monitor, "Audio/Source", NULL);

	use_gst_device_monitor = gst_device_monitor_start(monitor);
	if (!use_gst_device_monitor) {
		g_warning("Failed to start device monitor!");
	}

	/* TODO: Check if configuration is valid and usable */
	gstreamer_channels = channels;
	gstreamer_sample_rate = sample_rate;
	gstreamer_bits_per_sample = bits_per_sample;

	return 0;
}

/**
 * gstreamer_open:
 * @output: output device
 *
 * Open audio device
 *
 * Returns: private data or %NULL on error
 */
static void *gstreamer_open(gchar *output)
{
	RmProfile *profile = rm_profile_get_active();
	GstreamerPipes *pipes = NULL;
	GstElement *sink;
	GstElement *audio_sink;
	GstElement *audio_source;
	GstElement *pipe;
	GstElement *filter;
	GstElement *convert;
	GstElement *resample;
	GstCaps *filtercaps;
	GstDevice *output_device = NULL;
	GstDevice *input_device = NULL;
	GList *list;
	GList *gst_devices;
	gchar *output_name;
	gchar *input_name;
	gint ret;

	pipes = g_slice_alloc0(sizeof(GstreamerPipes));
	if (pipes == NULL) {
		return NULL;
	}

	/* Get devices */
	if (use_gst_device_monitor) {
		gst_devices = gst_device_monitor_get_devices(monitor);

		/* Get preferred input/output device names */
		output_name = output;
		input_name = g_settings_get_string(profile->settings, "audio-input");

		/* Find output device */
		for (list = gst_devices; list != NULL; list = list->next) {
			GstDevice *device = list->data;

			gchar *class = gst_device_get_device_class(device);
			gchar *name = gst_device_get_display_name(device);

			if (!strcmp(class, "Audio/Sink") && !strcmp(name, output_name)) {
				output_device = device;
			} else if (!strcmp(class, "Audio/Source") && !strcmp(name, input_name)) {
				input_device = device;
			}
		}

		if (output_device) {
			audio_sink = gst_device_create_element(output_device, NULL);
		} else {
			g_warning("Could not get requested audio output device, falling back to default");
			audio_sink = gst_element_factory_make("autoaudiosink", NULL);
		}

		if (input_device) {
			audio_source = gst_device_create_element(input_device, NULL);
		} else {
			g_warning("Could not get requested audio input device, falling back to default");
			audio_source = gst_element_factory_make("autoaudiosrc", NULL);
		}
	} else {
		g_debug("%s(): Using auto audio src/sink", __FUNCTION__);
		audio_sink = gst_element_factory_make("autoaudiosink", NULL);
		audio_source = gst_element_factory_make("autoaudiosrc", NULL);
	}

	/* Create output pipeline */
	if (audio_sink != NULL) {
		pipe = gst_pipeline_new("output");
		g_assert(pipe != NULL);

		GstElement *source = gst_element_factory_make("appsrc", "rm_src");
		g_assert(source != NULL);

		/* Removed this code part as it causes a crash on windows/wine */
		/*g_object_set(G_OBJECT(source),
			     "is-live", 1,
			     "format", 3,
			     "block", 1,
			     "max-bytes", SAMPLE_SIZE,
			     NULL);*/

		filter = gst_element_factory_make("capsfilter", "filter");

		filtercaps = gst_caps_new_simple("audio/x-raw",
						 "format", G_TYPE_STRING, "S16LE",
						 "channels", G_TYPE_INT, gstreamer_channels,
						 "rate", G_TYPE_INT, gstreamer_sample_rate,
						 NULL);

		g_object_set(G_OBJECT(filter), "caps", filtercaps, NULL);
		gst_caps_unref(filtercaps);

		convert = gst_element_factory_make("audioconvert", "convert");
		resample = gst_element_factory_make("audioresample", "resample");

		gst_bin_add_many(GST_BIN(pipe), source, filter, convert, resample, audio_sink, NULL);
		gst_element_link_many(source, filter, convert, resample, audio_sink, NULL);

		ret = gst_element_set_state(pipe, GST_STATE_PLAYING);
		if (ret == GST_STATE_CHANGE_FAILURE) {
			g_warning("Error: cannot start sink pipeline => %d", ret);
			return NULL;
		}

		pipes->out_pipe = pipe;
		pipes->out_bin = gst_bin_get_by_name(GST_BIN(pipe), "rm_src");
		gstreamer_set_buffer_output_size(pipe, SAMPLE_SIZE);
	}

	/* Create input pipeline */
	if (audio_source != NULL) {
		pipe = gst_pipeline_new("input");
		g_assert(pipe != NULL);

		sink = gst_element_factory_make("appsink", "rm_sink");
		g_assert(sink != NULL);

		filter = gst_element_factory_make("capsfilter", "filter");

		filtercaps = gst_caps_new_simple("audio/x-raw",
						 "format", G_TYPE_STRING, "S16LE",
						 "channels", G_TYPE_INT, gstreamer_channels,
						 "rate", G_TYPE_INT, gstreamer_sample_rate,
						 NULL);

		g_object_set(G_OBJECT(filter), "caps", filtercaps, NULL);
		gst_caps_unref(filtercaps);

		convert = gst_element_factory_make("audioconvert", "convert");
		resample = gst_element_factory_make("audioresample", "resample");

		gst_bin_add_many(GST_BIN(pipe), audio_source, filter, convert, resample, sink, NULL);
		gst_element_link_many(audio_source, filter, convert, resample, sink, NULL);

		ret = gst_element_set_state(pipe, GST_STATE_PLAYING);
		if (ret == GST_STATE_CHANGE_FAILURE) {
			g_warning("Error: cannot start source pipeline => %d", ret);
			return NULL;
		}

		pipes->in_pipe = pipe;
		pipes->in_bin = gst_bin_get_by_name(GST_BIN(pipe), "rm_sink");
	}

	pipes->adapter = gst_adapter_new();

	return pipes;
}

/**
 * gstreamer_write:
 * @priv: internal pipe data
 * @data: audio data
 * @size: size of audio data
 *
 * Write audio data
 *
 * Returns: bytes written or error code
 */
static gsize gstreamer_write(void *priv, guchar *data, gsize size)
{
	GstBuffer *buffer = NULL;
	GstreamerPipes *pipes = priv;
	gchar *tmp;

	tmp = g_malloc(size);
	memcpy((char*)tmp, (char*)data, size);

	buffer = gst_buffer_new_wrapped(tmp, size);
	gst_app_src_push_buffer(GST_APP_SRC(pipes->out_bin), buffer);

	return size;
}

/**
 * gstreamer_read:
 * @priv: internal pipe data
 * @data: audio data
 * @size: size of audio data
 *
 * Read audio data
 *
 * Returns: bytes read or error code
 */
static gsize gstreamer_read(void *priv, guchar *data, gsize size)
{
	GstSample *sample = NULL;
	GstreamerPipes *pipes = priv;
	GstElement *sink = pipes->in_bin;
	gsize read = 0;

	if (!sink) {
		return read;
	}

	sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
	if (sample == NULL) {
		return read;
	}

	gst_adapter_push(pipes->adapter, gst_sample_get_buffer(sample));
	read = MIN(gst_adapter_available(pipes->adapter), size);
	if (read != 0) {
		gst_adapter_copy(pipes->adapter, data, 0, read);
		gst_adapter_flush(pipes->adapter, read);
	}

	//gst_sample_unref(sample);

	return read;
}

/**
 * gstreamer_close:
 * @priv: private data
 *
 * Stop and remove pipeline
 *
 * Returns: error code
 */
int gstreamer_close(void *priv)
{
	GstreamerPipes *pipes = priv;

	if (pipes == NULL) {
		return 0;
	}

	GstElement *src = pipes->out_bin;
	if (src != NULL) {
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipes->out_pipe));
		gst_bus_add_watch(bus, gstreamer_pipeline_cleaner, pipes->out_pipe);
		gst_app_src_end_of_stream(GST_APP_SRC(src));
		gst_element_set_state(pipes->out_pipe, GST_STATE_NULL);
		gst_object_unref(bus);
		gst_object_unref(pipes->out_pipe);
		pipes->out_pipe = NULL;
	}

	if (pipes->in_pipe != NULL) {
		gst_element_set_state(pipes->in_pipe, GST_STATE_NULL);
		gst_object_unref(pipes->in_pipe);
		pipes->out_pipe = NULL;
	}

	g_slice_free(GstreamerPipes, pipes);
	pipes = NULL;

	return 0;
}

/**
 * gstremaer_shutdown:
 *
 * Shutdown gstreamer
 *
 * Returns: 0
 */
int gstreamer_shutdown(void)
{
	gst_device_monitor_stop(monitor);
	gst_object_unref(monitor);

	return 0;
}

/** audio definition */
RmAudio gstreamer = {
	"GStreamer",
	gstreamer_init,
	gstreamer_open,
	gstreamer_write,
	gstreamer_read,
	gstreamer_close,
	gstreamer_shutdown,
	gstreamer_detect_devices
};

/**
 * gstreamer_plugin_init:
 * @plugin: a #RmPlugin
 *
 * Activate plugin (add net event)
 *
 * Returns: %TRUE
 */
static gboolean gstreamer_plugin_init(RmPlugin *plugin)
{
	gst_init(NULL, NULL);

	g_setenv("PULSE_PROP_media.role", "phone", TRUE);
	g_setenv("PULSE_PROP_filter.want", "echo-cancel", TRUE);

	rm_audio_register(&gstreamer);

	return TRUE;
}

/**
 * gstreamer_plugin_shutdown:
 * @plugin: a #RmPlugin
 *
 * Deactivate plugin (remote net event)
 *
 * Returns: %TRUE
 */
static gboolean gstreamer_plugin_shutdown(RmPlugin *plugin)
{
	rm_audio_unregister(&gstreamer);

	return TRUE;
}

RM_PLUGIN(gstreamer);

