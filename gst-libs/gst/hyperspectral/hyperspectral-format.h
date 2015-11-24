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

#ifndef __GST_HYPERSPECTRAL_FORMAT_H__
#define __GST_HYPERSPECTRAL_FORMAT_H__

#include <gst/gst.h>

/**
 * GstHyperspectralCubeFormat:
 * @GST_HSPC_LAYOUT_UNKNOWN: Unknown or unset cube format
 * @GST_HSPC_LAYOUT_MULTIPLANE: Data is aligned based on wavelengths,
 *    with all data for a wavelength being stored in the same buffer region
 * @GST_HSPC_LAYOUT_INTERLEAVED: Data is stored on a "pixel" basis,
 *    with all wavelengths for a given pixel stored in the same data region
 *
 * The different ways a data cube is stored in a buffer.
 */

typedef enum {
  GST_HSPC_LAYOUT_UNKNOWN = -1,
  GST_HSPC_LAYOUT_MULTIPLANE = 0,
  GST_HSPC_LAYOUT_INTERLEAVED
} GstHyperspectralLayout;

const gchar *           gst_hspec_layout_to_string (GstHyperspectralLayout mode);
GstHyperspectralLayout  gst_hspec_layout_from_string (const gchar * mode);

#define GST_HYPERSPECTRAL_MEDIA_TYPE "video/hyperspectral-cube"
#define GST_HYPERSPECTRAL_FRACTION_RANGE "(fraction) [ 0, max ]"
#define GST_HYPERSPECTRAL_FORMATS_ALL "{ GRAY8, GRAY16_BE, GRAY16_LE }"
#define GST_HYPERSPECTRAL_CUBE_FORMATS_ALL "{ multiplane, interleaved }"

#define GST_HYPERSPECTRAL_CAPS_MAKE(format)                         \
    GST_HYPERSPECTRAL_MEDIA_TYPE ", "                                    \
      "format = (string) " format ", "                              \
      "width = " GST_VIDEO_SIZE_RANGE ", "                          \
      "height = " GST_VIDEO_SIZE_RANGE ", "                         \
      "wavelengths = " GST_VIDEO_SIZE_RANGE ", "                          \
      "pixel-aspect-ratio = " GST_HYPERSPECTRAL_FRACTION_RANGE ", " \
      "framerate = " GST_HYPERSPECTRAL_FRACTION_RANGE ", "          \
      "layout = " GST_HYPERSPECTRAL_CUBE_FORMATS_ALL

#define GST_HYPERSPECTRAL_CAPS_MAKE_WITH_ALL_FORMATS()              \
    GST_HYPERSPECTRAL_MEDIA_TYPE ", "                                    \
      "format = (string) " GST_HYPERSPECTRAL_FORMATS_ALL ", "       \
      "width = " GST_VIDEO_SIZE_RANGE ", "                          \
      "height = " GST_VIDEO_SIZE_RANGE ", "                         \
      "wavelengths = " GST_VIDEO_SIZE_RANGE ", "                          \
      "pixel-aspect-ratio = " GST_HYPERSPECTRAL_FRACTION_RANGE ", " \
      "framerate = " GST_HYPERSPECTRAL_FRACTION_RANGE ", "          \
      "layout = " GST_HYPERSPECTRAL_CUBE_FORMATS_ALL

GstCaps * gst_hspec_build_caps (gint data_cube_width, gint data_cube_height,
  gint data_cube_wavelengths, const gchar *fmtstr, const gchar *layoutstr,
  gint wavelengthno, gint *wavelengths);

#endif /* __GST_HYPERSPECTRAL_FORMAT_H__ */