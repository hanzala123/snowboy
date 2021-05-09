
/* GStreamer snowboy plugin
 * Copyright (C) <2021> Liam Wickins <liamw9534@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-plugin
 *
 * Snowboy hotword detector gst plugin.  Uses the snowboy detector library
 * and links results to the `hotword-detect` signal which should be monitored
 * from your application to determine when a hotword has been found.
 *
 * The plugin allows multiple models to be used at the same time using the
 * `models` property.  It should be a comma-separated list of model files e.g.,
 * models=file1.umdl,file2.umdl.
 *
 * The hotword-detect signal will provide an integer which is the model index e.g.,
 * 0=>file1, 1=>file2 etc.
 *
 * The plugin is designed to work in a larger pipeline structure that could be
 * used to perform additional audio processing.  For this reason the input data
 * on the sinkpad is also passed through to the srcpad.  If you don't need this
 * then just terminate with a `fakesink` element.
 *
 * The plugin only works with single channel audio S16 @ 16KHz as reflected by
 * the fixed caps.
 *
 * Note that the snowboy detector is a C++ library which means we have to compile
 * and link this plugin using a C++ compiler.  The gstreamer deps are wrapped in
 * an extern "C" declaration so that the boilerplate code compiles clean.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m pulsesrc ! snowboy ! fakesink
 * ]|
 * </refsect2>
 */
 
extern "C" {

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>
#include <string.h>

}

#include "snowboy-detect.h"

GST_DEBUG_CATEGORY_STATIC (snowboy_debug);
#define GST_CAT_DEFAULT snowboy_debug

#define GST_TYPE_SNOWBOY (gst_snowboy_get_type())
G_DECLARE_FINAL_TYPE (GstSnowboy, gst_snowboy,
    GST, SNOWBOY, GstAudioFilter)

struct _GstSnowboy {
  GstAudioFilter audiofilter;

  // Props
  gchar *resource;
  gchar *models;
  gchar *sensitivity;
  gfloat gain;
  gboolean listen;

  // Detector instance
  snowboy::SnowboyDetect *detector;
};


enum {
  HOTWORD_DETECT_SIGNAL,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_MODELS,
  PROP_RESOURCE,
  PROP_SENSITIVITY,
  PROP_GAIN,
  PROP_LISTEN,
};

G_DEFINE_TYPE (GstSnowboy, gst_snowboy,
    GST_TYPE_AUDIO_FILTER);

static guint gst_snowboy_signals[LAST_SIGNAL] = { 0 };

static void gst_snowboy_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_snowboy_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_snowboy_setup (GstAudioFilter * filter,
    const GstAudioInfo * info);
static GstFlowReturn gst_snowboy_filter (GstBaseTransform * bt,
    GstBuffer * outbuf, GstBuffer * inbuf);
static void gst_snowboy_finalize (GObject * obj);

/* For simplicity only support 16-bit pcm in native endianness for starters */
#define SUPPORTED_CAPS_STRING \
    GST_AUDIO_CAPS_MAKE(GST_AUDIO_NE(S16))

/* GObject vmethod implementations */
static void
gst_snowboy_class_init (GstSnowboyClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *btrans_class;
  GstAudioFilterClass *snowboy_class;
  GstCaps *caps;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  btrans_class = (GstBaseTransformClass *) klass;
  snowboy_class = (GstAudioFilterClass *) klass;

  gobject_class->set_property = gst_snowboy_set_property;
  gobject_class->get_property = gst_snowboy_get_property;

  g_object_class_install_property (gobject_class, PROP_RESOURCE,
      g_param_spec_string ("resource", "Snowboy resource",
    		  	  	  	   "Filename of snowboy resource file",
                           NULL,
                           G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MODELS,
		  g_param_spec_string ("models", "Snowboy models",
				  	  	  	   "Comma separated filename(s) of snowboy model file(s)",
		                       NULL,
		                       G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SENSITIVITY,
      g_param_spec_string ("sensitivity", "Model sensitivities",
    		  	  	  	   "Comma separated floating point value per model",
                           NULL,
                           G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_GAIN,
      g_param_spec_float ("gain", "Audio gain", "Input gain at detector",
                           0.0, 1.0, 1.0,
                           G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_LISTEN,
      g_param_spec_boolean ("listen", "Detector listen on/off",
    		  	  	  	  	"Indicates whether detector is listening or not",
    		  	  	  	  	TRUE,
							G_PARAM_READWRITE));

  // Signals
  gst_snowboy_signals[HOTWORD_DETECT_SIGNAL] = g_signal_new ("hotword-detect",
		G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL, NULL,
        G_TYPE_NONE, 1, G_TYPE_INT);

  /* this function will be called when the format is set before the
   * first buffer comes in, and whenever the format changes */
  snowboy_class->setup = gst_snowboy_setup;

  /* Need some clean up by adding finalize */
  gobject_class->finalize = gst_snowboy_finalize;

  /* Filter function for incoming audio */
  btrans_class->transform = gst_snowboy_filter;
  /* Set some basic metadata about your new element */
  gst_element_class_set_details_simple (element_class,
    "Snowboy",
    "Filter/Effect/Audio",
    "Hotword speech detection",
    "Liam Wickins <liamw9534@gmail.com>");

  caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, GST_AUDIO_NE(S16),
          "rate", G_TYPE_INT, 16000,
          "channels", G_TYPE_INT, 1, NULL);
  gst_audio_filter_class_add_pad_templates (snowboy_class, caps);
  gst_caps_unref (caps);
}

static void
gst_snowboy_init (GstSnowboy * snowboy)
{
  /* This function is called when a new filter object is created. You
   * would typically do things like initialise properties to their
   * default values here if needed. */
  snowboy->detector = nullptr;
  snowboy->resource = g_strdup("resources/common.res");
  snowboy->models = g_strdup("resources/models/snowboy.umdl");
  snowboy->sensitivity = g_strdup("0.5");
  snowboy->gain = 1.0;
  snowboy->listen = true;
}

static void gst_snowboy_finalize (GObject * obj)
{
  GstSnowboy *snowboy = GST_SNOWBOY (obj);
  if (snowboy->detector)
    delete snowboy->detector;
  g_free(snowboy->resource);
  g_free(snowboy->models);
  g_free(snowboy->sensitivity);
}

static void
gst_snowboy_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSnowboy *snowboy = GST_SNOWBOY (object);

  GST_OBJECT_LOCK (snowboy);
  switch (prop_id) {
    case PROP_RESOURCE:
      if (!g_value_get_string (value)) {
        g_warning ("resource property cannot be NULL");
        break;
      }
      g_free (snowboy->resource);
      snowboy->resource = g_strdup (g_value_get_string (value));
      break;
    case PROP_MODELS:
      if (!g_value_get_string (value)) {
        g_warning ("models property cannot be NULL");
        break;
      }
      g_free (snowboy->models);
      snowboy->models = g_strdup (g_value_get_string (value));
      break;
    case PROP_SENSITIVITY:
      snowboy->sensitivity = g_strdup (g_value_get_string (value));
      break;
    case PROP_GAIN:
      snowboy->gain = g_value_get_float (value);
      break;
    case PROP_LISTEN:
      snowboy->listen = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (snowboy);
}

static void
gst_snowboy_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSnowboy *snowboy = GST_SNOWBOY (object);

  GST_OBJECT_LOCK (snowboy);
  switch (prop_id) {
    case PROP_RESOURCE:
      g_value_set_string (value, snowboy->resource);
      break;
    case PROP_MODELS:
      g_value_set_string (value, snowboy->models);
      break;
    case PROP_SENSITIVITY:
      g_value_set_string (value, snowboy->sensitivity);
      break;
    case PROP_GAIN:
      g_value_set_float (value, snowboy->gain);
      break;
    case PROP_LISTEN:
      g_value_set_boolean (value, snowboy->listen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (snowboy);
}

static gboolean
gst_snowboy_setup (GstAudioFilter * filter,
    const GstAudioInfo * info)
{
  GstSnowboy *snowboy;
  GstAudioFormat fmt;
  gint chans, rate;

  snowboy = GST_SNOWBOY (filter);

  rate = GST_AUDIO_INFO_RATE (info);
  chans = GST_AUDIO_INFO_CHANNELS (info);
  fmt = GST_AUDIO_INFO_FORMAT (info);

  GST_INFO_OBJECT (snowboy, "input format %d (%s), rate %d, %d channels",
      fmt, GST_AUDIO_INFO_NAME (info), rate, chans);

  /* Create detector object and initialise it with props */
  snowboy->detector = new snowboy::SnowboyDetect(snowboy->resource, snowboy->models);
  snowboy->detector->SetSensitivity(snowboy->sensitivity);
  snowboy->detector->SetAudioGain(snowboy->gain);
  snowboy->detector->ApplyFrontend(false);

  GST_INFO_OBJECT (snowboy, "detector rate %d, %d channels, width %d",
                   snowboy->detector->SampleRate(),
                   snowboy->detector->NumChannels(),
                   snowboy->detector->BitsPerSample());

  /* The audio filter base class also saves the audio info in
   * GST_AUDIO_FILTER_INFO(snowboy) so it's automatically available
   * later from there as well */

  return TRUE;
}

/* You may choose to implement either a copying filter or an
 * in-place filter (or both).  Implementing only one will give
 * full functionality, however, implementing both will cause
 * audiofilter to use the optimal function in every situation,
 * with a minimum of memory copies. */

static GstFlowReturn
gst_snowboy_filter (GstBaseTransform * base_transform,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstSnowboy *snowboy = GST_SNOWBOY (base_transform);
  GstMapInfo map_in;
  GstMapInfo map_out;

  GST_LOG_OBJECT (snowboy, "push samples into snowboy detector");

  if (gst_buffer_map (inbuf, &map_in, GST_MAP_READ)) {
    if (gst_buffer_map (outbuf, &map_out, GST_MAP_WRITE)) {
      g_assert (map_out.size == map_in.size);
      memcpy (map_out.data, map_in.data, map_out.size);
      gst_buffer_unmap (outbuf, &map_out);
    }

    if (snowboy->listen) {
		int result = snowboy->detector->RunDetection(
				(const int16_t* const)map_in.data,
				map_in.size / 2);
		if (result > 0) {
		  GST_INFO_OBJECT (snowboy, "model index: %d", result - 1);
		  g_signal_emit (G_OBJECT (snowboy),
				  gst_snowboy_signals[HOTWORD_DETECT_SIGNAL],
				  0,
				  result - 1);
		}
    }

    gst_buffer_unmap (inbuf, &map_in);
  }

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  /* Register debug category for filtering log messages */
  GST_DEBUG_CATEGORY_INIT (snowboy_debug, "snowboy", 0,
      "Snowboy hotword detector");

  /* This is the name used in gst-launch-1.0 and gst_element_factory_make() */
  return gst_element_register (plugin, "snowboy", GST_RANK_NONE,
      GST_TYPE_SNOWBOY);
}

/* gstreamer looks for this structure to register plugins */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    snowboy,
    "Snowboy plugin",
    plugin_init,
    PACKAGE_VERSION,
    GST_LICENSE,
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
);
