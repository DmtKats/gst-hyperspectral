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

#ifndef _GST_HYPERSPECTRALDEC_H_
#define _GST_HYPERSPECTRALDEC_H_

#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/hyperspectral/hyperspectral.h>
G_BEGIN_DECLS

#define GST_TYPE_HYPERSPECTRALDEC   (gst_hyperspectraldec_get_type())
#define GST_HYPERSPECTRALDEC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HYPERSPECTRALDEC,GstHyperspectraldec))
#define GST_HYPERSPECTRALDEC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HYPERSPECTRALDEC,GstHyperspectraldecClass))
#define GST_IS_HYPERSPECTRALDEC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HYPERSPECTRALDEC))
#define GST_IS_HYPERSPECTRALDEC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HYPERSPECTRALDEC))

typedef struct _GstHyperspectraldec GstHyperspectraldec;
typedef struct _GstHyperspectraldecClass GstHyperspectraldecClass;

struct _GstHyperspectraldec
{
  GstVideoDecoder base_hyperspectraldec;
  GstHyperspectralInfo hinfo;

  GstVideoCodecState *output_state;
  void (*writefunc) (GstHyperspectraldec *dec,
    GstHyperspectralFrame *inframe, GstVideoFrame *outframe);

  gint wavelengthid;
  gint wavelengthpos;
};

struct _GstHyperspectraldecClass
{
  GstVideoDecoderClass base_hyperspectraldec_class;

  /* vfunc for writing data from source frame to sink frame */
};

GType gst_hyperspectraldec_get_type (void);

gboolean gst_hyperspectraldec_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif
