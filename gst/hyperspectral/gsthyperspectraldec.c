/* GStreamer
 * Copyright (C) 2015 Dimitrios Katsaros <patcherwork@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gsthyperspectraldec
 *
 * The hspecdec element takes a stream of video data formatted into a hyperspectral cube
 * and reverts it to an image where the hyperspectral data is layed out as defined by the
 * hyperspectral mosaic.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! hspecenc ! hspecdec ! fakesink
 * ]|
 * A simple example of converting to and from the hyperspectral format
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>
#include <string.h>
#include "gsthyperspectraldec.h"

GST_DEBUG_CATEGORY_STATIC (gst_hyperspectraldec_debug_category);
#define GST_CAT_DEFAULT gst_hyperspectraldec_debug_category

/* prototypes */


static void gst_hyperspectraldec_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_hyperspectraldec_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_hyperspectraldec_dispose (GObject * object);
static void gst_hyperspectraldec_finalize (GObject * object);
static gboolean gst_hyperspectraldec_src_event (GstVideoDecoder *decoder, GstEvent * event);
static gboolean gst_hyperspectraldec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_hyperspectraldec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);

enum
{
  PROP_0,
  PROP_WAVELENGTHID
};

/* pad templates */

static GstStaticPadTemplate gst_hyperspectraldec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_HYPERSPECTRAL_CAPS_MAKE_WITH_ALL_FORMATS())
    );

static GstStaticPadTemplate gst_hyperspectraldec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE("{ GRAY8, GRAY16_LE, GRAY16_BE }"))
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstHyperspectraldec, gst_hyperspectraldec, GST_TYPE_VIDEO_DECODER,
  GST_DEBUG_CATEGORY_INIT (gst_hyperspectraldec_debug_category, "hspecdec", 0,
  "debug category for hyperspectraldec element"));

static void
gst_hyperspectraldec_class_init (GstHyperspectraldecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoDecoderClass *video_decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&gst_hyperspectraldec_sink_template));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&gst_hyperspectraldec_src_template));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Hyperspectral decoder", "decoder/hyperspectral/Video",
      "Convertas a hyperspectral stream to a raw data stream",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  gobject_class->set_property = gst_hyperspectraldec_set_property;
  gobject_class->get_property = gst_hyperspectraldec_get_property;
  gobject_class->dispose = gst_hyperspectraldec_dispose;
  gobject_class->finalize = gst_hyperspectraldec_finalize;
  video_decoder_class->src_event = GST_DEBUG_FUNCPTR (gst_hyperspectraldec_src_event);
  video_decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_hyperspectraldec_set_format);
  video_decoder_class->handle_frame = GST_DEBUG_FUNCPTR (gst_hyperspectraldec_handle_frame);
  video_decoder_class->transform_meta = NULL;

  g_object_class_install_property (gobject_class, PROP_WAVELENGTHID,
      g_param_spec_int ("wavelengthid", "Wavelength ID",
          "Sets the number of the wavelength id to be shown", 0, G_MAXINT,
          0, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
}

static void
gst_hyperspectraldec_init (GstHyperspectraldec *dec)
{
  gst_hyperspectral_info_init(&dec->hinfo);
  dec->output_state = NULL;
  dec->writefunc = NULL;
  dec->wavelengthpos = 0;
  dec->wavelengthid = 0;
}

void
gst_hyperspectraldec_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstHyperspectraldec *hyperspectraldec = GST_HYPERSPECTRALDEC (object);

  GST_DEBUG_OBJECT (hyperspectraldec, "set_property");

  switch (property_id) {
    case PROP_WAVELENGTHID:
      hyperspectraldec->wavelengthid = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_hyperspectraldec_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstHyperspectraldec *hyperspectraldec = GST_HYPERSPECTRALDEC (object);

  GST_DEBUG_OBJECT (hyperspectraldec, "get_property");

  switch (property_id) {
    case PROP_WAVELENGTHID:
      g_value_set_int(value, hyperspectraldec->wavelengthid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_hyperspectraldec_dispose (GObject * object)
{
  GstHyperspectraldec *dec = GST_HYPERSPECTRALDEC (object);

  GST_DEBUG_OBJECT (dec, "dispose");

  gst_hyperspectral_info_clear(&dec->hinfo);
  if (dec->output_state)
    gst_video_codec_state_unref (dec->output_state);
  dec->output_state = NULL;
  dec->writefunc = NULL;
  dec->wavelengthpos = 0;
  dec->wavelengthid = 0;


  G_OBJECT_CLASS (gst_hyperspectraldec_parent_class)->dispose (object);
}

void
gst_hyperspectraldec_finalize (GObject * object)
{
  GstHyperspectraldec *dec = GST_HYPERSPECTRALDEC (object);

  GST_DEBUG_OBJECT (dec, "finalize");

  gst_hyperspectral_info_clear(&dec->hinfo);

  if (dec->output_state)
    gst_video_codec_state_unref (dec->output_state);
  dec->output_state = NULL;
  dec->writefunc = NULL;

  G_OBJECT_CLASS (gst_hyperspectraldec_parent_class)->finalize (object);
}

static gboolean
gst_hyperspectraldec_src_event (GstVideoDecoder *decoder, GstEvent * event)
{
  GstHyperspectraldec *dec = GST_HYPERSPECTRALDEC (decoder);


  switch (gst_navigation_event_get_type (event)) {
    case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
    {
      gint button;
      if(gst_navigation_event_parse_mouse_button_event(event, &button, NULL, NULL) ) {
        if (button==1) {
          if (dec->wavelengthpos > 0)
            dec->wavelengthpos--;
          else
            dec->wavelengthpos = dec->hinfo.wavelengths-1;
          GST_DEBUG("Left mouse click detected, setting wavelengthpos to %d, wavelength id: %d", dec->wavelengthpos,
            dec->hinfo.mosaic.spectras[dec->wavelengthpos]);
        }
        else if (button==3){
          if (dec->hinfo.wavelengths-1 > dec->wavelengthpos)
            dec->wavelengthpos++;
          else
            dec->wavelengthpos = 0;
          GST_DEBUG("Right mouse click detected, setting wavelengthpos to %d, wavelength id: %d", dec->wavelengthpos,
            dec->hinfo.mosaic.spectras[dec->wavelengthpos]);
        }
      }
      break;
    }
    case GST_NAVIGATION_EVENT_KEY_PRESS:
    {
      const gchar *keystr;
      if (gst_navigation_event_parse_key_event(event, &keystr)) {
        if (strncmp (keystr, "Left", 4) == 0) {
          if (dec->wavelengthpos > 0)
            dec->wavelengthpos--;
          else
            dec->wavelengthpos = dec->hinfo.wavelengths-1;
          GST_DEBUG("Left key press detected, setting wavelengthpos to %d, wavelength id: %d", dec->wavelengthpos,
            dec->hinfo.mosaic.spectras[dec->wavelengthpos]);
        }
        else if (strncmp (keystr, "Right", 5) == 0) {
          if (dec->hinfo.wavelengths-1 > dec->wavelengthpos)
            dec->wavelengthpos++;
          else
            dec->wavelengthpos = 0;
          GST_DEBUG("Right key press detected, setting wavelengthpos to %d, wavelength id: %d", dec->wavelengthpos,
            dec->hinfo.mosaic.spectras[dec->wavelengthpos]);
        }
      }
      break;
    }
    default:
      break;
  }
  return GST_VIDEO_DECODER_CLASS (gst_hyperspectraldec_parent_class)->src_event (decoder, event);
}

/* performs unpacking of hyperspectral cube to a 2d image */
static void
from_cube_to_image_1byte_multiplanar(GstHyperspectraldec *dec, GstHyperspectralFrame *inframe, GstVideoFrame *outframe)
{
  guint8 *restrict src = (guint8*) GST_HSPEC_FRAME_WAVELENGTH_DATA_MULTIPLANE(inframe, dec->wavelengthpos);
  guint8 *restrict target = (guint8*) GST_VIDEO_FRAME_PLANE_DATA (outframe, 0);
  gint stride = GST_VIDEO_FRAME_PLANE_STRIDE(outframe,0);
  gint i, j;
  for (j=0; j<outframe->info.height; j++) {
    for (i=0; i<outframe->info.width; i++) {
      target[i + j*stride] = src[i + j*outframe->info.width];
    }
  }
}

static void
from_cube_to_image_1byte_interleaved(GstHyperspectraldec *dec, GstHyperspectralFrame *inframe, GstVideoFrame *outframe)
{
  guint8 *restrict src = (guint8*) (inframe->data + dec->wavelengthpos);
  guint8 *restrict target = (guint8*) GST_VIDEO_FRAME_PLANE_DATA(outframe, 0);
  gint stride = GST_VIDEO_FRAME_PLANE_STRIDE(outframe,0);
  gint i, j;
  for (j=0; j<outframe->info.height; j++) {
    for (i=0; i<outframe->info.width; i++) {
      target[i + j*stride] = src[(i + j*outframe->info.width)*inframe->info.wavelengths];
    }
  }
}

static void
from_cube_to_image_2byte_multiplanar(GstHyperspectraldec *dec, GstHyperspectralFrame *inframe, GstVideoFrame *outframe)
{
  guint16 *restrict src = (guint16*) GST_HSPEC_FRAME_WAVELENGTH_DATA_MULTIPLANE(inframe, dec->wavelengthpos);
  guint16 *restrict target = (guint16*) GST_VIDEO_FRAME_PLANE_DATA (outframe, 0);
  gint stride = GST_VIDEO_FRAME_PLANE_STRIDE(outframe,0)/2;
  gint i, j;
  for (j=0; j<outframe->info.height; j++) {
    for (i=0; i<outframe->info.width; i++) {
      target[i + j*stride] = src[i + j*outframe->info.width];
    }
  }
}

static void
from_cube_to_image_2byte_interleaved(GstHyperspectraldec *dec, GstHyperspectralFrame *inframe, GstVideoFrame *outframe)
{
  guint16 *restrict src = (guint16*) (inframe->data + dec->wavelengthpos*2);
  guint16 *restrict target = (guint16*) GST_VIDEO_FRAME_PLANE_DATA (outframe, 0);
  gint stride = GST_VIDEO_FRAME_PLANE_STRIDE(outframe,0)/2;
  gint i, j;
  for (j=0; j<outframe->info.height; j++) {
    for (i=0; i<outframe->info.width; i++) {
      target[i + j*stride] = src[(i + j*outframe->info.width)*inframe->info.wavelengths];
    }
  }
}

static gboolean
gst_hyperspectraldec_set_format (GstVideoDecoder * decoder, GstVideoCodecState * state)
{
  GstHyperspectraldec *dec = GST_HYPERSPECTRALDEC (decoder);
  int i;
  gboolean res = FALSE;

  GST_DEBUG("Hyperspectral caps: %" GST_PTR_FORMAT, state->caps);
  if(!gst_hyperspectral_info_from_caps(&dec->hinfo, state->caps))
    return FALSE;
  if (dec->output_state)
    gst_video_codec_state_unref (dec->output_state);
  dec->output_state = gst_video_decoder_set_output_state(decoder,
    dec->hinfo.format, dec->hinfo.width, dec->hinfo.height, state);

  dec->output_state->caps = gst_video_info_to_caps (&dec->output_state->info);
  /* check if wavelength id is within range of availabe wavelengths */

  for (i=0; i<dec->hinfo.wavelengths; i++) {
    if (dec->hinfo.mosaic.spectras[i] == dec->wavelengthid) {
      dec->wavelengthpos = i;
      res = TRUE;
      break;
    }
  }

  if (!res && (dec->wavelengthid != 0))
    GST_WARNING("Could not find wavelength id '%d' in hyperspectral wavelengths, defaulting to '%d'",
      dec->wavelengthid, dec->hinfo.mosaic.spectras[dec->wavelengthpos]);
  else
    GST_DEBUG("Selected wavelength id '%d'",
      dec->hinfo.mosaic.spectras[dec->wavelengthpos]);
  /* select function for copying*/
  switch (dec->hinfo.format) {
    case GST_VIDEO_FORMAT_GRAY8:
      if (dec->hinfo.layout == GST_HSPC_LAYOUT_MULTIPLANE) {
        GST_DEBUG("Selecting 'from_cube_to_image_1byte_multiplanar' writefunc for %s",
          gst_video_format_to_string(dec->hinfo.format));
        dec->writefunc = from_cube_to_image_1byte_multiplanar;
      } else if (dec->hinfo.layout == GST_HSPC_LAYOUT_INTERLEAVED) {
        GST_DEBUG("Selecting 'from_cube_to_image_1byte_interleaved' writefunc for %s",
          gst_video_format_to_string(dec->hinfo.format));
        dec->writefunc = from_cube_to_image_1byte_interleaved;
      } else {
        GST_ERROR("Unknown layout '%s'", gst_hspec_layout_to_string(dec->hinfo.layout));
        return FALSE;
      }
      break;
    case GST_VIDEO_FORMAT_GRAY16_BE:
    case GST_VIDEO_FORMAT_GRAY16_LE:
      if (dec->hinfo.layout == GST_HSPC_LAYOUT_MULTIPLANE) {
        GST_DEBUG("Selecting 'from_cube_to_image_2byte_multiplanar' writefunc for %s",
          gst_video_format_to_string(dec->hinfo.format));
        dec->writefunc = from_cube_to_image_2byte_multiplanar;
      } else if (dec->hinfo.layout == GST_HSPC_LAYOUT_INTERLEAVED) {
        GST_DEBUG("Selecting 'from_cube_to_image_2byte_interleaved' writefunc for %s",
          gst_video_format_to_string(dec->hinfo.format));
        dec->writefunc = from_cube_to_image_2byte_interleaved;
      } else {
        GST_ERROR("Unknown layout '%s'", gst_hspec_layout_to_string(dec->hinfo.layout));
        return FALSE;
      }
      break;
    default:
      GST_ERROR("Unhandled format of type %s", gst_video_format_to_string(dec->hinfo.format));
      return FALSE;
  }
  GST_DEBUG("Calculated caps: %" GST_PTR_FORMAT, dec->output_state->caps);
  return TRUE;
}

static GstFlowReturn
gst_hyperspectraldec_handle_frame (GstVideoDecoder * decoder, GstVideoCodecFrame * frame)
{
  GstHyperspectraldec *dec = GST_HYPERSPECTRALDEC (decoder);

  GstHyperspectralFrame hframe;
  GstVideoFrame outframe;
  GstFlowReturn ret;

  if (!gst_hyperspectral_frame_map(&hframe, &dec->hinfo, frame->input_buffer,
    GST_MAP_READ)) {
    GST_ERROR_OBJECT (dec, "Could not map hyperspectral frame");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  /* allocate a new frame for the image */
  ret = gst_video_decoder_allocate_output_frame (decoder,
      frame);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    GST_DEBUG_OBJECT (dec, "failed to acquire buffer");
    gst_hyperspectral_frame_unmap (&hframe);
    gst_video_codec_frame_unref (frame);
    return ret;
  }

  if (!gst_video_frame_map (&outframe, &dec->output_state->info, frame->output_buffer,
      GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (dec, "Could not map video frame");
    gst_buffer_unref (frame->output_buffer);
    gst_hyperspectral_frame_unmap (&hframe);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  if (!dec->writefunc) {
    GST_ERROR_OBJECT (dec, "Write function has not been set! (Decoder not initialized?)");
    gst_video_frame_unmap (&outframe);
    gst_buffer_unref (frame->output_buffer);
    gst_hyperspectral_frame_unmap (&hframe);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  /* at this point everything should be set and I can now perform processing */
  dec->writefunc(dec, &hframe, &outframe);
  /* processing is complete, send the buffer */
  gst_video_decoder_finish_frame(decoder, frame);
  gst_video_frame_unmap (&outframe);
  gst_hyperspectral_frame_unmap (&hframe);

  return GST_FLOW_OK;
}

gboolean
gst_hyperspectraldec_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "hspecdec", GST_RANK_NONE,
      GST_TYPE_HYPERSPECTRALDEC);
}
