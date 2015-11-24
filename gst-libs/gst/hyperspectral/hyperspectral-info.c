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

#include <string.h>
#include "hyperspectral-info.h"

void gst_hyperspectral_info_init(GstHyperspectralInfo *info)
{
  memset(info, 0, sizeof(GstHyperspectralInfo));
}

void gst_hyperspectral_info_clear(GstHyperspectralInfo *info)
{
  clear_mosaic(&info->mosaic);
  memset(info, 0, sizeof(GstHyperspectralInfo));
}

static gboolean
get_byte_size_from_format(GstVideoFormat fmt, guint *size)
{
  switch (fmt) {
    case GST_VIDEO_FORMAT_GRAY8:
      *size = 1;
      break;
    case GST_VIDEO_FORMAT_GRAY16_LE:
    case GST_VIDEO_FORMAT_GRAY16_BE:
      *size = 2;
      break;
    default:
      GST_ERROR("Unhandled format type '%s'", gst_video_format_to_string(fmt));
      return FALSE;
  }

  return TRUE;
}

gboolean
gst_hyperspectral_info_from_caps(GstHyperspectralInfo *info, const GstCaps *caps)
{
  GstStructure *structure;
  const gchar *s;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  GstHyperspectralLayout layout = GST_HSPC_LAYOUT_UNKNOWN;

  gint width = 0, height = 0, wavelengths = 0;
  guint bytesize = 0;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (caps != NULL, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  GST_DEBUG ("parsing hyperspectral info from caps %" GST_PTR_FORMAT, caps);

  structure = gst_caps_get_structure (caps, 0);
  if (gst_structure_has_name (structure, "video/hyperspectral-cube")) {
    if (!(s = gst_structure_get_string (structure, "format")))
      goto no_format;

    format = gst_video_format_from_string (s);
    if (format == GST_VIDEO_FORMAT_UNKNOWN)
      goto unknown_format;

    /* retrieve the cube layout */
    if (!(s = gst_structure_get_string (structure, "layout")))
      goto no_layout;

    layout = gst_hspec_layout_from_string (s);
    if (layout == GST_HSPC_LAYOUT_UNKNOWN)
      goto unknown_layout;
  } else {
    goto wrong_name;
  }

  /* get width, height and wavelengths */
  if (!gst_structure_get_int (structure, "width", &width))
    goto no_width;
  if (!gst_structure_get_int (structure, "height", &height))
    goto no_height;
  if (!gst_structure_get_int (structure, "wavelengths", &wavelengths))
    goto no_wavelengths;

  if (!get_byte_size_from_format(format, &bytesize))
    return FALSE;
  gst_hyperspectral_info_init(info);
  if (!load_mosaic_from_caps(&info->mosaic, caps))
    return FALSE;

  info->width = width;
  info->height = height;
  info->wavelengths = wavelengths;
  info->bytesize = bytesize;
  info->wavelength_elems = info->width * info->height;
  info->cube_elems = info->width * info->height * info->wavelengths;
  info->wavelength_size = info->width * info->height *
                    info->bytesize;
  info->cube_size = info->width * info->height *
                    info->wavelengths * info->bytesize;
  info->format = format;
  info->layout = layout;
  return TRUE;

wrong_name:
  {
    GST_ERROR ("wrong name '%s', expected video/hyperspectral-cube",
        gst_structure_get_name (structure));
    return FALSE;
  }
no_format:
  {
    GST_ERROR ("no format given");
    return FALSE;
  }
unknown_format:
  {
    GST_ERROR ("unknown format '%s' given", s);
    return FALSE;
  }
no_layout:
  {
    GST_ERROR ("no hyperspectral cube layout given");
    return FALSE;
  }
unknown_layout:
  {
    GST_ERROR ("unknown hyperspectral cube layout '%s'", s);
    return FALSE;
  }
no_width:
  {
    GST_ERROR ("no width property given");
    return FALSE;
  }
no_height:
  {
    GST_ERROR ("no height property given");
    return FALSE;
  }
no_wavelengths:
  {
    GST_ERROR ("no wavelength property given");
    return FALSE;
  }
}