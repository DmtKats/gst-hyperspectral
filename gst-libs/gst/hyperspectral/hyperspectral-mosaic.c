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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hyperspectral-mosaic.h"

const static gint DEFAULT_MOSAIC_5x5[5][5] = {
      {739,753,727,713,688},
      {923,930,917,909,944},
      {843,854,833,821,939},
      {883,893,874,864,949},
      {790,802,778,765,673}
    };

const static gint DEFAULT_MOSAIC_BGGR[2][2] = {
      {440,530},
      {531,630},
    };

const static gint DEFAULT_MOSAIC_GBRG[2][2] = {
      {530,440},
      {630,531},
    };

const static gint DEFAULT_MOSAIC_GRBG[2][2] = {
      {530,630},
      {440,531},
    };

const static gint DEFAULT_MOSAIC_RGGB[2][2] = {
      {630,531},
      {530,440},
    };

typedef struct
{
  gint id;
  guint width;
  guint height;
  const gpointer mosaic;
} DefaultEntry;

static const DefaultEntry default_entries[] = {
  {SPECTRA_DEFAULT_5x5, 5, 5, &DEFAULT_MOSAIC_5x5},
  {SPECTRA_DEFAULT_BGGR, 2, 2, &DEFAULT_MOSAIC_BGGR},
  {SPECTRA_DEFAULT_GBRG, 2, 2, &DEFAULT_MOSAIC_GBRG},
  {SPECTRA_DEFAULT_GRBG, 2, 2, &DEFAULT_MOSAIC_GRBG},
  {SPECTRA_DEFAULT_RGGB, 2, 2, &DEFAULT_MOSAIC_RGGB},
};

#define DEFAULT_COUNT (G_N_ELEMENTS (default_entries))

void
init_mosaic(SpectralInfo *mosaic) {
  memset(mosaic, 0, sizeof(SpectralInfo));
}

void
clear_mosaic(SpectralInfo *mosaic) {
  mosaic->size = 0;
  if (mosaic->spectras)
    free(mosaic->spectras);
  mosaic->spectras = NULL;
}

gboolean
set_mosaic_to_default(SpectralInfo *mosaic, gint default_type, gint *dwidth, gint *dheight) {
  GST_DEBUG("Setting mosaic to default");
  int i;
  for (i=0; i<DEFAULT_COUNT; i++) {
    if (default_type==default_entries[i].id){
      GST_DEBUG("Selected default %d", i);
      gint width = default_entries[i].width;
      gint height = default_entries[i].height;

      if (dwidth)
        *dwidth = width;
      if (dheight)
        *dheight = height;

      mosaic->size = default_entries[i].width * default_entries[i].height;
      /* malloc the mosaic */
      mosaic->spectras = malloc (height * width * sizeof(gint));

      if (!mosaic->spectras) {
        GST_ERROR("Unable to malloc memory of size %lu",
          (height * width * sizeof(gint)));
        clear_mosaic(mosaic);
        return FALSE;
      }

      memcpy(mosaic->spectras, default_entries[i].mosaic, mosaic->size * sizeof(gint));
      return TRUE;
    }
  }
  GST_ERROR("Could not find a default for id %d", default_type);
  return FALSE;
}

static gboolean
parse_mosaic(SpectralInfo *mosaic, FILE *f, gint *width, gint *height) {

  int w, h, i, j, val;
  gint *tspec_mosaic = NULL;


  if ( fscanf(f, "%d,%d=", &w, &h) != 2) {
    GST_WARNING("Could not parse width and height");
    fclose(f);
    return FALSE;
  }

  tspec_mosaic = malloc (w * h * sizeof(gint));

  if (!tspec_mosaic) {
    GST_WARNING("Unable to malloc memory of size %lu", (w * h * sizeof(gint)));
    return FALSE;
  }


  for (j=0; j<h; j++) {
    GST_DEBUG("Parsing line %d", j+1);
    for (i=0; i<w; i++) {
      GST_DEBUG("Parsing row %d", i+1);
      if(fscanf(f, "%d", &val)!=1)
        goto err_close;
      else {
        GST_DEBUG("Value for %d,%d:%d", i,j,val);
        tspec_mosaic[i + j*w] = val;
        GST_DEBUG("Stored value:%d",tspec_mosaic[i + j*w]);
      }
      if (j!=w){
        if(fscanf(f, ",")!=0)
          goto err_close;
      }
    }
    if(fscanf(f, ":")!=0)
      goto err_close;
  }

  if (mosaic->spectras)
    free (mosaic->spectras);

  mosaic->spectras = tspec_mosaic;
  mosaic->size = h * w;

  if (width)
    *width  = w;
  if (height)
    *height = h;


  return TRUE;

err_close:
  {
    if (tspec_mosaic)
      free (tspec_mosaic);
    GST_ERROR("Error while parsing spectra intensities");
    return FALSE;
  }
}

gboolean
read_mosaic_from_file (SpectralInfo *mosaic, char * filename, gint *width, gint *height) {

  gboolean res;
  FILE *f = fopen(filename, "r");

  if (f == NULL) {
    GST_WARNING("Cannot open mosaic file at %s", filename);
    GST_WARNING("Reason: %s", g_strerror (errno));
    return FALSE;
  }

  res = parse_mosaic(mosaic, f, width, height);
  fclose(f);

  return res;
}

gboolean
read_mosaic_from_string (SpectralInfo *mosaic, GString * string, gint *width, gint *height) {

  gboolean res;
  FILE *f = fmemopen(string->str, string->len, "r");

  if (f == NULL) {
    GST_WARNING("Cannot create buffer from string");
    GST_WARNING("Reason: %s", g_strerror (errno));
    return FALSE;
  }

  res = parse_mosaic(mosaic, f, width, height);
  fclose(f);

  return res;
}

gboolean load_mosaic_from_caps(SpectralInfo *mosaic, const GstCaps *caps) {

  GstStructure *structure;
  const GValue *array, *value;
  gint size = 0, i;
  gint *tmosaic;

  g_return_val_if_fail (mosaic != NULL, FALSE);
  g_return_val_if_fail (caps != NULL, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);


  GST_DEBUG ("Getting spectra info from caps %" GST_PTR_FORMAT, caps);

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_has_name (structure, "video/hyperspectral-cube")) {
    goto wrong_name;
  }
  /* check for wavelength size */
  if (!gst_structure_get_int (structure, "wavelengths", &size))
    goto no_wavelengths;

  /* next, get the mosaic list */
  array = gst_structure_get_value (structure,
                          "wavelength_ids");

  if (array == NULL)
    goto no_wavelength_list;
  else if (G_VALUE_TYPE(array)!=GST_TYPE_ARRAY)
    goto wrong_type;
  else if (gst_value_array_get_size(array) != size)
    goto wrong_size;

  /* initialize structure to hold mosaic and set it*/
  tmosaic = malloc (size * sizeof(gint));
  for (i=0; i<size; i++) {
    value = gst_value_array_get_value(array, i);
    if (G_VALUE_TYPE(value) != G_TYPE_INT)
      goto wrong_member_type;
    tmosaic[i] = g_value_get_int (value);
  }

  /* if we got here everything went well so set the struct*/
  clear_mosaic(mosaic);
  mosaic->size = size;
  mosaic->spectras = tmosaic;

  return TRUE;

wrong_name:
  {
    GST_ERROR ("wrong name '%s', expected video/hyperspectral-cube",
        gst_structure_get_name (structure));
    return FALSE;
  }
no_wavelengths:
  {
    GST_ERROR ("no 'wavelengths' found");
    return FALSE;
  }
no_wavelength_list:
  {
    GST_ERROR ("no 'wavelength_ids' found");
    return FALSE;
  }
wrong_type:
  {
    GST_ERROR ("wrong type '%s', expected GST_TYPE_ARRAY",
        G_VALUE_TYPE_NAME(array));
    return FALSE;
  }
wrong_size:
  {
    GST_ERROR ("wrong wavelength array size, was expecting %d got %u",
        size, gst_value_array_get_size(array));
    return FALSE;
  }
wrong_member_type:
  {
    GST_ERROR ("wrong type '%s' for array member, expected GST_TYPE_INT",
        G_VALUE_TYPE_NAME(array));
    free (tmosaic);
    return FALSE;
  }
}
