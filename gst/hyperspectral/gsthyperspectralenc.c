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
 * SECTION:element-gsthyperspectralenc
 *
 * The hsepcenc element encodes a video stream into a hyperspectral data cube.
 * This is neccesary for hyperspectral sensors that output their data as an image
 * with the pixel layout being defined by a mosaic.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! hspecenc ! hspecdec ! fakesink
 * ]|
 * A simple pipeline that encodes and decodes an image.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>
#include <stdlib.h>
#include <string.h>
#include <gst/hyperspectral/hyperspectral-format.h>
#include "gsthyperspectralenc.h"


GST_DEBUG_CATEGORY_STATIC (gst_hyperspectralenc_debug_category);
#define GST_CAT_DEFAULT gst_hyperspectralenc_debug_category

/* prototypes */


static void gst_hyperspectralenc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_hyperspectralenc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_hyperspectralenc_dispose (GObject * object);
static void gst_hyperspectralenc_finalize (GObject * object);

static gboolean gst_hyperspectralenc_set_format (GstVideoEncoder *encoder, GstVideoCodecState *state);
static GstFlowReturn gst_hyperspectralenc_handle_raw_buffer (GstVideoEncoder *encoder, GstVideoCodecFrame *frame);
static GstFlowReturn gst_hyperspectralenc_handle_video_frame (GstVideoEncoder *encoder, GstVideoCodecFrame *frame);

enum
{
  PROP_0,
  PROP_MOSAIC_STR,
  PROP_MOSAIC_PATH
};

/* defaults */

#define DEFAULT_LAYOUT GST_HSPC_LAYOUT_MULTIPLANE

/* pad templates */
static GstStaticPadTemplate gst_hyperspectralenc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_HYPERSPECTRAL_CAPS_MAKE_WITH_ALL_FORMATS())
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstHyperspectralenc, gst_hyperspectralenc, GST_TYPE_VIDEO_ENCODER,
  GST_DEBUG_CATEGORY_INIT (gst_hyperspectralenc_debug_category, "hspecenc", 0,
  "debug category for hyperspectralenc element"));

typedef struct {
  gchar category[32];
  gchar format[32];
} FormatDesc;

static const FormatDesc formats [] = {
  {"video/x-raw", "GRAY8"},
  {"video/x-raw", "GRAY16_BE"},
  {"video/x-raw", "GRAY16_LE"},
  {"video/x-bayer", "bggr"},
  {"video/x-bayer", "rggb"},
  {"video/x-bayer", "gbrg"},
  {"video/x-bayer", "grbg"},
};

#define FORMAT_COUNT (G_N_ELEMENTS (formats))

static GstCaps *
make_all_caps()
{
  GstStructure *structure;
  GstCaps *caps = gst_caps_new_empty ();
  int i;

  for (i=0; i<FORMAT_COUNT; i++) {

    structure = gst_structure_new (formats[i].category,
        "format", G_TYPE_STRING, formats[i].format,
        "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
    gst_caps_append_structure (caps, structure);
  }
  caps = gst_caps_simplify(caps);
  return caps;
}

static void
gst_hyperspectralenc_class_init (GstHyperspectralencClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoEncoderClass *video_encoder_class = GST_VIDEO_ENCODER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&gst_hyperspectralenc_src_template));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new (
        "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        make_all_caps()));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Hyperspectra encoder", "encoder/hyperspectral/Video",
      "Converts a raw video stream to a hyperspectral video cube",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  gobject_class->set_property = gst_hyperspectralenc_set_property;
  gobject_class->get_property = gst_hyperspectralenc_get_property;
  gobject_class->dispose = gst_hyperspectralenc_dispose;
  gobject_class->finalize = gst_hyperspectralenc_finalize;

  video_encoder_class->handle_frame = GST_DEBUG_FUNCPTR (gst_hyperspectralenc_handle_video_frame);
  video_encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_hyperspectralenc_set_format);
  video_encoder_class->transform_meta = NULL;

  g_object_class_install_property (gobject_class, PROP_MOSAIC_STR,
      g_param_spec_string ("mosaicstr", "MosaicString",
          "A string reprisentation of the mosaic. "
          "The format is: width,height=val,val,val:... "
          "Elements are spearated by commas,\n "
          "                      "
          " rows are separated by ':'. The mosaic is expected "
          "to be a square. Also the mosaic must fit perfectly the sensor."
          , "None",
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MOSAIC_PATH,
      g_param_spec_string ("mosaicfile", "MosaicPath",
          "The location of the file from which to load the mosaic. "
          "The file format is the same as that of the mosaicstr parameter", "None",
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));


}

static void
gst_hyperspectralenc_init (GstHyperspectralenc *enc)
{
  init_mosaic(&enc->mosaic);
  enc->data_cube_width = 0;
  enc->data_cube_height = 0;
  enc->data_cube_wavelengths = 0;
  enc->data_wavelength_elems = 0;
  enc->data_cube_size = 0;
  enc->frame_elems = 0;
  enc->data_byte_size = 0;

  enc->input_state = NULL;
  enc->writefunc = NULL;
  enc->mosaic_str = NULL;
  enc->mosaic_path = NULL;

  enc->layout = DEFAULT_LAYOUT;
}

void
gst_hyperspectralenc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstHyperspectralenc *enc = GST_HYPERSPECTRALENC (object);
  GString *str;
  GST_DEBUG_OBJECT (enc, "set_property");

  switch (property_id) {
    case PROP_MOSAIC_STR:
      str = g_string_new(g_value_get_string(value));
      if (!read_mosaic_from_string(&enc->mosaic, str,
          &enc->mosaic_width, &enc->mosaic_height)) {
        g_string_free(str, TRUE);
      } else {
        if (enc->mosaic_str)
          g_string_free(enc->mosaic_str, TRUE);
        enc->mosaic_str = str;
      }
      break;
    case PROP_MOSAIC_PATH:
      str = g_string_new(g_value_get_string(value));
      if (!read_mosaic_from_file(&enc->mosaic, str->str,
          &enc->mosaic_width, &enc->mosaic_height)) {
        g_string_free(str, TRUE);
      } else {
        if (enc->mosaic_path)
          g_string_free(enc->mosaic_path, TRUE);
        enc->mosaic_path = str;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_hyperspectralenc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstHyperspectralenc *enc = GST_HYPERSPECTRALENC (object);

  GST_DEBUG_OBJECT (enc, "get_property");

  switch (property_id) {
    case PROP_MOSAIC_STR:
      if(enc->mosaic_str)
        g_value_set_string(value, enc->mosaic_str->str);
      else
        g_value_set_string(value, "");
      break;
    case PROP_MOSAIC_PATH:
      if(enc->mosaic_path)
        g_value_set_string(value, enc->mosaic_path->str);
      else
        g_value_set_string(value, "");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}
void
gst_hyperspectralenc_dispose (GObject * object)
{
  GstHyperspectralenc *enc = GST_HYPERSPECTRALENC (object);

  GST_DEBUG_OBJECT (enc, "dispose");

  clear_mosaic(&enc->mosaic);
  enc->data_cube_width = 0;
  enc->data_cube_height = 0;
  enc->data_cube_wavelengths = 0;
  enc->data_wavelength_elems = 0;
  enc->data_cube_size = 0;
  enc->frame_elems = 0;
  enc->data_byte_size = 0;
  enc->writefunc = NULL;
  enc->layout = DEFAULT_LAYOUT;

  if (enc->input_state)
    gst_video_codec_state_unref (enc->input_state);
  enc->input_state = NULL;

  if (enc->mosaic_path)
    g_string_free(enc->mosaic_path, TRUE);
  enc->mosaic_path = NULL;
  if (enc->mosaic_str)
    g_string_free(enc->mosaic_str, TRUE);
  enc->mosaic_str = NULL;

  G_OBJECT_CLASS (gst_hyperspectralenc_parent_class)->dispose (object);
}

void
gst_hyperspectralenc_finalize (GObject * object)
{
  GstHyperspectralenc *enc = GST_HYPERSPECTRALENC (object);

  GST_DEBUG_OBJECT (enc, "finalizing...");

  clear_mosaic(&enc->mosaic);
  if (enc->input_state)
    gst_video_codec_state_unref (enc->input_state);

  if (enc->mosaic_path)
    g_string_free(enc->mosaic_path, TRUE);

  if (enc->mosaic_str)
    g_string_free(enc->mosaic_str, TRUE);

  G_OBJECT_CLASS (gst_hyperspectralenc_parent_class)->finalize (object);
}

static void
write_cube_to_buffer_1byte_multiplanar(GstHyperspectralenc *enc, gpointer input_buffer, gpointer output_buffer,
  gint width, gint height, gint stride) {
  guint8 *restrict inp = (guint8*) input_buffer;
  guint8 *restrict outp = (guint8*) output_buffer;
  gint i, j, wavelengthid;

  for (j=0; j<height; j++) {
    for (i=0; i<width; i++) {
      wavelengthid = i%enc->mosaic_width + (j%enc->mosaic_height)*enc->mosaic_width;
      outp[wavelengthid*enc->data_wavelength_elems +  i/enc->mosaic_width +
        (j/enc->mosaic_height)*enc->data_cube_width] = inp[i + j*stride];
    }
  }
}

static void
write_cube_to_buffer_1byte_interleaved(GstHyperspectralenc *enc, gpointer input_buffer, gpointer output_buffer,
  gint width, gint height, gint stride) {
  guint8 *restrict inp = (guint8*) input_buffer;
  guint8 *restrict outp = (guint8*) output_buffer;
  gint i, j;

  for (j=0; j<height; j++) {
    for (i=0; i<width; i++) {
      outp[(i/enc->mosaic_width + (j/enc->mosaic_height)*enc->data_cube_width)*enc->data_cube_wavelengths +
            i%enc->mosaic_width + (j%enc->mosaic_height)*enc->mosaic_width] = inp[i + j*stride];
    }
  }
}

static void
write_cube_to_buffer_2byte_multiplanar(GstHyperspectralenc *enc, gpointer input_buffer, gpointer output_buffer,
  gint width, gint height, gint stride) {
  guint16 *restrict inp = (guint16*) input_buffer;
  guint16 *restrict outp = (guint16*) output_buffer;
  gint i, j, wavelengthid;

  for (j=0; j<height; j++) {
    for (i=0; i<width; i++) {
      wavelengthid = i%enc->mosaic_width + (j%enc->mosaic_height)*enc->mosaic_width;
      outp[wavelengthid*enc->data_wavelength_elems +  i/enc->mosaic_width +
        (j/enc->mosaic_height)*enc->data_cube_width] = inp[i + j*stride];
    }
  }
}

static void
write_cube_to_buffer_2byte_interleaved(GstHyperspectralenc *enc, gpointer input_buffer, gpointer output_buffer,
  gint width, gint height, gint stride) {
  guint16 *restrict inp = (guint16*) input_buffer;
  guint16 *restrict outp = (guint16*) output_buffer;
  gint i, j;

  for (i=0; i<width; i++) {
    for (j=0; j<height; j++) {
      outp[(i/enc->mosaic_width + (j/enc->mosaic_height)*enc->data_cube_width)*enc->data_cube_wavelengths +
            i%enc->mosaic_width + (j%enc->mosaic_height)*enc->mosaic_width] = inp[i + j*stride];
    }
  }
}

static gboolean gst_hyperspectralenc_set_format (GstVideoEncoder *encoder, GstVideoCodecState *state)
{
  GstHyperspectralenc *enc = GST_HYPERSPECTRALENC (encoder);
  GstVideoEncoderClass *klass = GST_VIDEO_ENCODER_GET_CLASS(encoder);
  GstVideoInfo *info = &state->info;
  GstVideoCodecState *output_state;
  gint srcwidth = enc->srcwidth = GST_VIDEO_INFO_WIDTH (info);
  gint srcheight = enc->srcheight = GST_VIDEO_INFO_HEIGHT (info);
  GstStructure *instruct, *peerstruct = NULL;
  gint defaultid = SPECTRA_DEFAULT_UNKNOWN;
  const gchar *fmtstr, *layoutstr = NULL;
  const GValue *value;
  GstHyperspectralLayout layout;
  GstCaps *peercaps, *generated_caps;

  GST_DEBUG("Video frame caps: %" GST_PTR_FORMAT, state->caps);

  /* get caps from peer to determine cube layout */
  peercaps = gst_pad_peer_query_caps (encoder->srcpad, NULL);
  /* this handles both fixated and non fixated layout*/
  if (peercaps) {
    if (!gst_caps_is_empty (peercaps) && !gst_caps_is_any (peercaps))
      peerstruct = gst_caps_get_structure (peercaps, 0);
    else {
      layoutstr = gst_hspec_layout_to_string(enc->layout);
      GST_WARNING("Peer caps are empty, defaulting layout to %s", layoutstr);
    }
  } else {
    layoutstr = gst_hspec_layout_to_string(enc->layout);
    GST_WARNING("Unable to retrieve caps from peer, defaulting layout to %s", layoutstr);
  }

  if (peerstruct) {
    GST_DEBUG("Caps of src peer: %" GST_PTR_FORMAT, peercaps);
    value = gst_structure_get_value (peerstruct, "layout");
    if (value) {
      if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
          layoutstr = gst_structure_get_string (peerstruct, "layout");
      }
      /* if layout is not fixated, pick the first one */
      else if (G_VALUE_TYPE (value) == GST_TYPE_LIST) {
        value = gst_value_list_get_value (value, 0);
        if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
          layoutstr = g_value_get_string (value);
        }
      }
    }
    if (layoutstr == NULL) {
      layoutstr = gst_hspec_layout_to_string (enc->layout);
      GST_WARNING("Unable to retrieve layout from peercaps, "
        "defaulting layout to %s", layoutstr);
    } else {
      layout = gst_hspec_layout_from_string (layoutstr);
      if (layout == GST_HSPC_LAYOUT_UNKNOWN) {
        GST_ERROR("Unknown hyperspectral cube layout '%s'", layoutstr);
        gst_caps_unref (peercaps);
        return FALSE;
      }
      enc->layout = layout;
      /* get a static scring so we can unref the caps */
      layoutstr = gst_hspec_layout_to_string (enc->layout);
    }
    gst_caps_unref (peercaps);
  }
  else {
    layoutstr = gst_hspec_layout_to_string(enc->layout);
    GST_WARNING("Peer caps are empty, defaulting layout to %s", layoutstr);
  }
  GST_DEBUG("Layout selected: %s", layoutstr);

  /* set the format for the output caps */
  instruct = gst_caps_get_structure (state->caps, 0);
  if (gst_structure_has_name (instruct, "video/x-raw")) {
    klass->handle_frame = GST_DEBUG_FUNCPTR (gst_hyperspectralenc_handle_video_frame);
    switch (GST_VIDEO_INFO_FORMAT (info)) {
      case GST_VIDEO_FORMAT_GRAY8:
        enc->data_byte_size = 1;
        if (enc->layout == GST_HSPC_LAYOUT_MULTIPLANE) {
          GST_DEBUG("Selecting 'write_cube_to_buffer_1byte_multiplanar' writefunc for %s",
            GST_VIDEO_INFO_NAME(info));
          enc->writefunc = write_cube_to_buffer_1byte_multiplanar;
        } else if (enc->layout == GST_HSPC_LAYOUT_INTERLEAVED) {
          GST_DEBUG("Selecting 'write_cube_to_buffer_1byte_interleaved' writefunc for %s",
            GST_VIDEO_INFO_NAME(info));
          enc->writefunc = write_cube_to_buffer_1byte_interleaved;
        } else {
          GST_ERROR("Unknown layout '%s'", layoutstr);
          return FALSE;
        }
        break;
      case GST_VIDEO_FORMAT_GRAY16_BE:
      case GST_VIDEO_FORMAT_GRAY16_LE:
        enc->data_byte_size = 2;
        if (enc->layout == GST_HSPC_LAYOUT_MULTIPLANE) {
          GST_DEBUG("Selecting 'write_cube_to_buffer_2byte_multiplanar' writefunc for %s",
            GST_VIDEO_INFO_NAME(info));
          enc->writefunc = write_cube_to_buffer_2byte_multiplanar;
        } else if (enc->layout == GST_HSPC_LAYOUT_INTERLEAVED) {
          GST_DEBUG("Selecting 'write_cube_to_buffer_2byte_interleaved' writefunc for %s",
            GST_VIDEO_INFO_NAME(info));
          enc->writefunc = write_cube_to_buffer_2byte_interleaved;
        } else {
          GST_ERROR("Unknown layout '%s'", layoutstr);
          return FALSE;
        }
        break;
      case GST_VIDEO_FORMAT_UNKNOWN:
        GST_ERROR("Unknown format detected");
        return FALSE;
      default:
        GST_ERROR("Unhandled format of type %s", GST_VIDEO_INFO_NAME(info));
        return FALSE;
    }
    defaultid = SPECTRA_DEFAULT_5x5;
    fmtstr = gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (info));
  }
  else if (gst_structure_has_name (instruct, "video/x-bayer")) {
    klass->handle_frame = GST_DEBUG_FUNCPTR (gst_hyperspectralenc_handle_raw_buffer);
    enc->data_byte_size = 1;
    fmtstr =  gst_structure_get_string (instruct, "format");
    if (enc->layout == GST_HSPC_LAYOUT_MULTIPLANE) {
      GST_DEBUG("Selecting 'write_cube_to_buffer_1byte_multiplanar' writefunc for %s",
        fmtstr);
      enc->writefunc = write_cube_to_buffer_1byte_multiplanar;
    } else if (enc->layout == GST_HSPC_LAYOUT_INTERLEAVED) {
      GST_DEBUG("Selecting 'write_cube_to_buffer_1byte_interleaved' writefunc for %s",
        fmtstr);
      enc->writefunc = write_cube_to_buffer_1byte_interleaved;
    } else {
      GST_ERROR("Unknown layout '%s'", layoutstr);
      return FALSE;
    }
    if (g_str_equal (fmtstr, "bggr")) {
      defaultid = SPECTRA_DEFAULT_BGGR;
    } else if (g_str_equal (fmtstr, "gbrg")) {
      defaultid = SPECTRA_DEFAULT_GBRG;
    } else if (g_str_equal (fmtstr, "grbg")) {
      defaultid = SPECTRA_DEFAULT_GRBG;
    } else if (g_str_equal (fmtstr, "rggb")) {
      defaultid = SPECTRA_DEFAULT_RGGB;
    } else {
      GST_ERROR("Unknown bayer format '%s'", fmtstr);
      return FALSE;
    }
    /*setting output format to gray8 since bayer formats are in 8 bit for every color*/
    fmtstr = gst_video_format_to_string (GST_VIDEO_FORMAT_GRAY8);
  }
  else {
    GST_ERROR("Unhandled video format type %s", gst_structure_get_name (instruct));
    return FALSE;
  }

  /*if the mosaic has not been defined use the default*/
  if (!enc->mosaic.spectras) {
    set_mosaic_to_default(&enc->mosaic, defaultid,
      &enc->mosaic_width, &enc->mosaic_height);
  }

  /* test that the size of the mosaic fits the caps correctly */
  if(srcwidth % enc->mosaic_width != 0 ||
     srcheight % enc->mosaic_height !=0) {
    GST_ERROR_OBJECT (enc, "Hyperspectral encoder expects the image size "
      "and the hyperspectral mosaic to be exact fits, mosaic: %dx%d, image %dx%d",
      enc->mosaic_width, enc->mosaic_height, srcwidth, srcheight);
    return FALSE;
  }

  enc->data_cube_width = srcwidth / enc->mosaic_width;
  enc->data_cube_height = srcheight / enc->mosaic_height;
  enc->data_cube_wavelengths = enc->mosaic_width * enc->mosaic_height;
  enc->data_wavelength_elems = enc->data_cube_width * enc->data_cube_height;
  enc->data_cube_size = enc->data_cube_width * enc->data_cube_height *
                        enc->data_cube_wavelengths * enc->data_byte_size;
  enc->frame_elems = info->size/enc->data_byte_size;
  if (enc->input_state)
    gst_video_codec_state_unref (enc->input_state);
  enc->input_state = gst_video_codec_state_ref (state);

  generated_caps = gst_hspec_build_caps (enc->data_cube_width,
    enc->data_cube_height, enc->data_cube_wavelengths,
    fmtstr, layoutstr, enc->mosaic.size, enc->mosaic.spectras);
  output_state = gst_video_encoder_set_output_state (encoder,
        generated_caps, state);

  output_state->info.width = enc->data_cube_width;
  output_state->info.height = enc->data_cube_height;
  output_state->info.stride[0] = enc->data_cube_width;
  GST_DEBUG("Calculated caps: %" GST_PTR_FORMAT, output_state->caps);
  gst_video_codec_state_unref (output_state);
  return TRUE;
}

static GstFlowReturn
gst_hyperspectralenc_handle_video_frame (GstVideoEncoder *encoder, GstVideoCodecFrame *frame)
{
  GstHyperspectralenc *henc = GST_HYPERSPECTRALENC (encoder);
  GstVideoFrame vframe;
  GstMapInfo outbuffinfo;
  GstFlowReturn ret;

  if(gst_video_encoder_allocate_output_frame (encoder,
    frame, henc->data_cube_size) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (encoder, "Could not allocate buffer");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
  /* error handling and sanity checks */
  if (!gst_video_frame_map (&vframe, &henc->input_state->info, frame->input_buffer,
      GST_MAP_READ)) {
    GST_ERROR_OBJECT (encoder, "Could not map video frame");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  if (!gst_buffer_map (frame->output_buffer, &outbuffinfo, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (encoder, "Could not map video frame");
    gst_video_frame_unmap (&vframe);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  if (outbuffinfo.size < henc->data_cube_size)
      goto invalid_size;

  if (!henc->writefunc) {
    GST_ERROR_OBJECT (encoder, "Write function has not been set! (Encoder not initialized?)");
    gst_video_frame_unmap (&vframe);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  henc->writefunc(henc, GST_VIDEO_FRAME_PLANE_DATA (&vframe, 0), outbuffinfo.data,
    GST_VIDEO_INFO_WIDTH (&henc->input_state->info),
    GST_VIDEO_INFO_HEIGHT (&henc->input_state->info),
    GST_VIDEO_INFO_COMP_STRIDE(&henc->input_state->info, 0)/henc->data_byte_size);
  gst_buffer_unmap (frame->output_buffer, &outbuffinfo);
  gst_video_frame_unmap (&vframe);

  ret = gst_video_encoder_finish_frame (encoder, frame);

  return ret;

invalid_size:
  {
    GST_ERROR_OBJECT (encoder, "Error in data buffer size, %" G_GSIZE_FORMAT " < %d",
      outbuffinfo.size, henc->data_cube_size);
    gst_buffer_unmap (frame->output_buffer, &outbuffinfo);
    gst_video_frame_unmap (&vframe);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_hyperspectralenc_handle_raw_buffer (GstVideoEncoder *encoder, GstVideoCodecFrame *frame)
{
  GstHyperspectralenc *henc = GST_HYPERSPECTRALENC (encoder);
  GstMapInfo inbuffinfo, outbuffinfo;
  GstFlowReturn ret;

  if(gst_video_encoder_allocate_output_frame (encoder,
    frame, henc->data_cube_size) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (encoder, "Could not allocate buffer");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
  /* error handling and sanity checks */
  if (!gst_buffer_map (frame->input_buffer, &inbuffinfo, GST_MAP_READ)) {
    GST_ERROR_OBJECT (encoder, "Could not map video frame");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  if (!gst_buffer_map (frame->output_buffer, &outbuffinfo, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (encoder, "Could not map video frame");
    gst_buffer_unmap (frame->input_buffer, &inbuffinfo);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  if (outbuffinfo.size < henc->data_cube_size)
      goto invalid_size;

  if (!henc->writefunc) {
    GST_ERROR_OBJECT (encoder, "Write function has not been set! (Encoder not initialized?)");
    gst_buffer_unmap (frame->input_buffer, &inbuffinfo);
    gst_buffer_unmap (frame->output_buffer, &outbuffinfo);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  henc->writefunc(henc, inbuffinfo.data, outbuffinfo.data,
    henc->srcwidth, henc->srcheight, henc->srcwidth);
  gst_buffer_unmap (frame->output_buffer, &outbuffinfo);
  gst_buffer_unmap (frame->input_buffer, &inbuffinfo);
  ret = gst_video_encoder_finish_frame (encoder, frame);

  return ret;

invalid_size:
  {
    GST_ERROR_OBJECT (encoder, "Error in data buffer size, %" G_GSIZE_FORMAT " < %d",
      outbuffinfo.size, henc->data_cube_size);
    gst_buffer_unmap (frame->output_buffer, &outbuffinfo);
    gst_buffer_unmap (frame->input_buffer, &inbuffinfo);
    gst_video_codec_frame_unref (frame);
    return FALSE;
  }
}

gboolean
gst_hyperspectralenc_plugin_init (GstPlugin * plugin)
{

  return gst_element_register (plugin, "hspecenc", GST_RANK_NONE,
      GST_TYPE_HYPERSPECTRALENC);
}
