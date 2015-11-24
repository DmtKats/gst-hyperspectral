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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_HYPERSPECTRALENC_H_
#define _GST_HYPERSPECTRALENC_H_

#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>
#include <gst/hyperspectral/hyperspectral.h>

G_BEGIN_DECLS

#define GST_TYPE_HYPERSPECTRALENC   (gst_hyperspectralenc_get_type())
#define GST_HYPERSPECTRALENC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HYPERSPECTRALENC,GstHyperspectralenc))
#define GST_HYPERSPECTRALENC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HYPERSPECTRALENC,GstHyperspectralencClass))
#define GST_IS_HYPERSPECTRALENC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HYPERSPECTRALENC))
#define GST_IS_HYPERSPECTRALENC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HYPERSPECTRALENC))

enum
{
  GST_HSPEC_UNKNOWN,
  GST_HSPEC_XRAW,
  GST_HSPEC_XBAYER,
} frame_type;

typedef struct _GstHyperspectralenc GstHyperspectralenc;
typedef struct _GstHyperspectralencClass GstHyperspectralencClass;

struct _GstHyperspectralenc
{
  GstVideoEncoder base_hyperspectralenc;

  GString *mosaic_path;
  GString *mosaic_str;
  SpectralInfo mosaic;
  gint data_byte_size;
  gint data_cube_width;
  gint data_cube_height;
  gint data_cube_wavelengths;
  gint data_wavelength_elems;
  gint data_cube_size;
  gint frame_elems;

  gint mosaic_width;
  gint mosaic_height;

  gint srcwidth;
  gint srcheight;

  GstHyperspectralLayout layout;

  GstVideoCodecState *input_state;

  /* vfunc for writing data from source frame to sink frame */
  void (*writefunc) (GstHyperspectralenc *enc, gpointer input_buffer, gpointer output_buffer,
    gint width, gint height, gint stride);
};

struct _GstHyperspectralencClass
{
  GstVideoEncoderClass base_hyperspectralenc_class;
};

GType gst_hyperspectralenc_get_type (void);

gboolean gst_hyperspectralenc_plugin_init (GstPlugin * plugin);
G_END_DECLS

#endif
