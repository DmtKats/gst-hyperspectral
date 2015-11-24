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


#ifndef __HYPERSPECTRAL_MOSAIC_H__
#define __HYPERSPECTRAL_MOSAIC_H__

#include <gst/gst.h>

enum {
    SPECTRA_DEFAULT_UNKNOWN,
    SPECTRA_DEFAULT_5x5,
    SPECTRA_DEFAULT_BGGR,
    SPECTRA_DEFAULT_GBRG,
    SPECTRA_DEFAULT_GRBG,
    SPECTRA_DEFAULT_RGGB,
} defualt_types;

/*
 * Helper interface for handling spectra mosaics
 */

typedef struct _spectras
{
  gint size;
  gint *spectras;
} SpectralInfo;


void init_mosaic(SpectralInfo *mosaic);
void clear_mosaic(SpectralInfo *mosaic);
gboolean set_mosaic_to_default(SpectralInfo *mosaic, gint type, gint *width, gint *height);
gboolean read_mosaic_from_file (SpectralInfo *mosaic, char * filename, gint *width, gint *height);
gboolean read_mosaic_from_string (SpectralInfo *mosaic, GString * string, gint *width, gint *height);
gboolean load_mosaic_from_caps(SpectralInfo *mosaic, const GstCaps *caps);

#endif