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


/*
  provides interface with basic hyperspectral info handling
 */

#ifndef __GST_HYPERSPECTRAL_INFO_H__
#define __GST_HYPERSPECTRAL_INFO_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include "hyperspectral-mosaic.h"
#include "hyperspectral-format.h"

typedef struct _GstHyperspectrlInfo GstHyperspectralInfo;

struct _GstHyperspectrlInfo {

  gint width;
  gint height;
  gint wavelengths;
  gint wavelength_elems;
  gint cube_elems;
  gsize wavelength_size;
  gsize cube_size;
  gsize bytesize;
  GstVideoFormat format;
  GstHyperspectralLayout layout;

  SpectralInfo mosaic;

};

void        gst_hyperspectral_info_init(GstHyperspectralInfo *info);

void        gst_hyperspectral_info_clear(GstHyperspectralInfo *info);

gboolean    gst_hyperspectral_info_from_caps(
    GstHyperspectralInfo *info, const GstCaps *caps);

#endif