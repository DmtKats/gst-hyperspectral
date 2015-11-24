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

/* helper library for mapping hyperspectral frames */

#ifndef __HYPERSPECTRAL_FRAME_H__
#define __HYPERSPECTRAL_FRAME_H__

#include <gst/gst.h>
#include <gst/hyperspectral/hyperspectral-info.h>

typedef struct _GstHyperspectralFrame GstHyperspectralFrame;


struct _GstHyperspectralFrame {

  GstHyperspectralInfo info;

  GstBuffer *buffer;

  gpointer  data;
  GstMapInfo map;

};

gboolean  gst_hyperspectral_frame_map     (GstHyperspectralFrame *frame, GstHyperspectralInfo *info,
                                           GstBuffer *buffer, GstMapFlags flags);
void      gst_hyperspectral_frame_unmap   (GstHyperspectralFrame *frame);

#define GST_HSPEC_FRAME_WAVELENGTH_DATA_MULTIPLANE(frame,b) ((frame)->data + (frame)->info.wavelength_size * b)


#endif