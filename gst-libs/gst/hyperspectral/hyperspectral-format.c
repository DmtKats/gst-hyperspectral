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
#include "hyperspectral-format.h"


static const gchar *cube_layout[] = {
  "multiplane",
  "interleaved"
};


const gchar *
gst_hspec_layout_to_string (GstHyperspectralLayout mode)
{
  if (((guint) mode) >= G_N_ELEMENTS (cube_layout))
    return NULL;

  return cube_layout[mode];
}

GstHyperspectralLayout
gst_hspec_layout_from_string (const gchar * mode)
{
  gint i;
  for (i = 0; i < G_N_ELEMENTS (cube_layout); i++) {
    if (g_str_equal (cube_layout[i], mode))
      return i;
  }
  return GST_HSPC_LAYOUT_UNKNOWN;
}

static void
add_wavelength_list_to_struct (GstStructure * structure,
  gint wavelengthno, gint *wavelengths)
{
  GValue array = { 0 };
  GValue value = { 0 };
  gint i;
  /* put copies of the buffers in a fixed list */
  g_value_init (&array, GST_TYPE_ARRAY);

  for (i=0; i< wavelengthno; i++) {
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, wavelengths[i]);
    gst_value_array_append_value (&array, &value);
    g_value_unset (&value);
  }

  gst_structure_take_value (structure, "wavelength_ids", &array);
}

GstCaps *
gst_hspec_build_caps (gint data_cube_width, gint data_cube_height,
  gint data_cube_wavelengths, const gchar *fmtstr,
  const gchar *layoutstr, gint wavelengthno,
  gint *wavelengths) {
  GstCaps *caps = gst_caps_new_empty_simple (GST_HYPERSPECTRAL_MEDIA_TYPE);
  GstStructure *outstruct = gst_caps_get_structure (caps, 0);;
  gst_structure_set(outstruct,
    "width", G_TYPE_INT, data_cube_width,
    "height", G_TYPE_INT, data_cube_height,
    "wavelengths", G_TYPE_INT, data_cube_wavelengths,
    "format", G_TYPE_STRING, fmtstr,
    "layout", G_TYPE_STRING, layoutstr,
    NULL);
  add_wavelength_list_to_struct(outstruct, wavelengthno, wavelengths);
  return caps;
}
