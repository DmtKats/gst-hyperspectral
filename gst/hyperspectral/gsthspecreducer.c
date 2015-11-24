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
 * SECTION:element-gsthspecreducer
 *
 * The hspecreducer element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! hspecreducer ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include "gsthspecreducer.h"

GST_DEBUG_CATEGORY_STATIC (gst_hspec_reducer_debug_category);
#define GST_CAT_DEFAULT gst_hspec_reducer_debug_category

/* prototypes */


static void gst_hspec_reducer_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_hspec_reducer_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_hspec_reducer_dispose (GObject * object);
static void gst_hspec_reducer_finalize (GObject * object);

static GstCaps *gst_hspec_reducer_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_hspec_reducer_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean gst_hspec_reducer_accept_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps);
static gboolean gst_hspec_reducer_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_hspec_reducer_transform_size (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, gsize size, GstCaps * othercaps,
    gsize * othersize);
static gboolean gst_hspec_reducer_start (GstBaseTransform * trans);
static GstFlowReturn gst_hspec_reducer_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);

enum
{
  PROP_0,
  PROP_INCLUSION_STR,
  PROP_EXCLUSION_STR,
};

/* pad templates */

static GstStaticPadTemplate gst_hspec_reducer_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_HYPERSPECTRAL_CAPS_MAKE_WITH_ALL_FORMATS())
    );

static GstStaticPadTemplate gst_hspec_reducer_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_HYPERSPECTRAL_CAPS_MAKE_WITH_ALL_FORMATS())
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstHspecReducer, gst_hspec_reducer, GST_TYPE_BASE_TRANSFORM,
  GST_DEBUG_CATEGORY_INIT (gst_hspec_reducer_debug_category, "hspec-reducer", 0,
  "debug category for hspecreducer element"));

static void
gst_hspec_reducer_class_init (GstHspecReducerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&gst_hspec_reducer_src_template));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&gst_hspec_reducer_sink_template));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Hyperspectral decoder", "decoder/hyperspectral/Video",
      "Convertas a hyperspectral stream to a raw data stream",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  gobject_class->set_property = gst_hspec_reducer_set_property;
  gobject_class->get_property = gst_hspec_reducer_get_property;
  gobject_class->dispose = gst_hspec_reducer_dispose;
  gobject_class->finalize = gst_hspec_reducer_finalize;
  base_transform_class->transform_caps = GST_DEBUG_FUNCPTR (gst_hspec_reducer_transform_caps);
  base_transform_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_hspec_reducer_fixate_caps);
  base_transform_class->accept_caps = GST_DEBUG_FUNCPTR (gst_hspec_reducer_accept_caps);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_hspec_reducer_set_caps);
  base_transform_class->transform_size = GST_DEBUG_FUNCPTR (gst_hspec_reducer_transform_size);
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_hspec_reducer_start);
  base_transform_class->transform = GST_DEBUG_FUNCPTR (gst_hspec_reducer_transform);
#if 0
  g_object_class_install_property (gobject_class, PROP_INCLUSION_LIST,
      g_param_spec_value_array ("inclist",
          "inclusion list",
          "Array containing the wavelength ids that should be included. "
          "Either an inclusion or an exclusion can be defined but not both.",
          g_param_spec_int ("ival",
              "inclusion id", "inclusion id", 0, G_MAXINT, 0,
              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_EXCLUSION_LIST,
      g_param_spec_value_array ("exclist",
          "exclusion list",
          "Array containing the wavelength ids that should be excluded. "
          "Either an inclusion or an exclusion can be defined but not both.",
          g_param_spec_int ("eval",
              "exclusion id", "exclusion id", 0, G_MAXINT, 0,
              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
#endif
  g_object_class_install_property (gobject_class, PROP_INCLUSION_STR,
      g_param_spec_string ("incstr", "Inclusion String",
          "String reprisentation of an Array containing the wavelength ids that should be included. "
          "The string is formatted as: inclist=wavelengthid,wavelengthid,wavelengthid... "
          "Either an inclusion or an exclusion can be defined but not both.",
          "", G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

 g_object_class_install_property (gobject_class, PROP_EXCLUSION_STR,
      g_param_spec_string ("excstr", "Exclusion String",
          "String reprisentation of an Array containing the wavelength ids that should be excluded. "
          "The string is formatted as: inclist=wavelengthid,wavelengthid,wavelengthid... "
          "Either an inclusion or an exclusion can be defined but not both.",
          "", G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

}

static void
gst_hspec_reducer_init (GstHspecReducer *hspecreducer)
{
  gst_hyperspectral_info_init(&hspecreducer->ininfo);
  gst_hyperspectral_info_init(&hspecreducer->outinfo);
  hspecreducer->excstr = NULL;
  hspecreducer->incstr = NULL;
  hspecreducer->exclist = NULL;
  hspecreducer->inclist = NULL;
  hspecreducer->wavelength_pos = NULL;
}

void
gst_hspec_reducer_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstHspecReducer *hspecreducer = GST_HSPEC_REDUCER (object);

  GST_DEBUG_OBJECT (hspecreducer, "set_property");

  switch (property_id) {
    case PROP_INCLUSION_STR:
      if (hspecreducer->excstr) {
        GST_WARNING ("Exclusion list has already been loaded, overriding with inclusion list");
        g_string_free(hspecreducer->excstr, TRUE);
        hspecreducer->excstr = NULL;
        if (hspecreducer->exclist) {
          g_array_free (hspecreducer->exclist, TRUE);
          hspecreducer->exclist = NULL;
        }
      }
      if (hspecreducer->incstr)
        g_string_free(hspecreducer->incstr, TRUE);
      hspecreducer->incstr = g_string_new(g_value_get_string(value));
      break;
    case PROP_EXCLUSION_STR:
      if(hspecreducer->incstr) {
        GST_WARNING ("Inclusion list has already been loaded, overriding with exclusion list");
        g_string_free(hspecreducer->incstr, TRUE);
        hspecreducer->incstr = NULL;
        if (hspecreducer->inclist) {
          g_array_free (hspecreducer->inclist, TRUE);
          hspecreducer->inclist = NULL;
        }
      }
      if(hspecreducer->excstr)
        g_string_free(hspecreducer->excstr, TRUE);
      hspecreducer->excstr = g_string_new(g_value_get_string(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_hspec_reducer_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstHspecReducer *hspecreducer = GST_HSPEC_REDUCER (object);

  GST_DEBUG_OBJECT (hspecreducer, "get_property");

  switch (property_id) {
    case PROP_INCLUSION_STR:
      if(hspecreducer->incstr)
        g_value_set_string(value, hspecreducer->incstr->str);
      else
        g_value_set_string(value, "");
      break;
    case PROP_EXCLUSION_STR:
      if(hspecreducer->excstr)
        g_value_set_string(value, hspecreducer->excstr->str);
      else
        g_value_set_string(value, "");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_hspec_reducer_dispose (GObject * object)
{
  GstHspecReducer *hspecreducer = GST_HSPEC_REDUCER (object);

  GST_DEBUG_OBJECT (hspecreducer, "dispose");

  if(hspecreducer->incstr)
    g_string_free(hspecreducer->incstr, TRUE);
  hspecreducer->incstr = NULL;

  if(hspecreducer->excstr)
    g_string_free(hspecreducer->excstr, TRUE);
  hspecreducer->excstr = NULL;

  if (hspecreducer->inclist) {
    g_array_free (hspecreducer->inclist, TRUE);
  }
  hspecreducer->inclist = NULL;

  if (hspecreducer->exclist) {
    g_array_free (hspecreducer->exclist, TRUE);
  }
  hspecreducer->exclist = NULL;

  if (hspecreducer->wavelength_pos)
    g_array_free (hspecreducer->wavelength_pos, TRUE);
  hspecreducer->wavelength_pos = NULL;

  G_OBJECT_CLASS (gst_hspec_reducer_parent_class)->dispose (object);
}

void
gst_hspec_reducer_finalize (GObject * object)
{
  GstHspecReducer *hspecreducer = GST_HSPEC_REDUCER (object);

  GST_DEBUG_OBJECT (hspecreducer, "finalize");

  gst_hyperspectral_info_clear(&hspecreducer->ininfo);
  gst_hyperspectral_info_clear(&hspecreducer->outinfo);

  if(hspecreducer->incstr)
    g_string_free(hspecreducer->incstr, TRUE);
  hspecreducer->incstr = NULL;

  if(hspecreducer->excstr)
    g_string_free(hspecreducer->excstr, TRUE);
  hspecreducer->excstr = NULL;

  if (hspecreducer->inclist) {
    g_array_free (hspecreducer->inclist, TRUE);
  }
  hspecreducer->inclist = NULL;

  if (hspecreducer->exclist) {
    g_array_free (hspecreducer->exclist, TRUE);
  }
  hspecreducer->exclist = NULL;

  if (hspecreducer->wavelength_pos)
    g_array_free (hspecreducer->wavelength_pos, TRUE);
  hspecreducer->exclist = NULL;

  G_OBJECT_CLASS (gst_hspec_reducer_parent_class)->finalize (object);
}

static GstCaps *
gst_hspec_reducer_transform_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, GstCaps * filter)
{
  GstHspecReducer *redu = GST_HSPEC_REDUCER (trans);
  GstCaps *othercaps;

  GST_DEBUG_OBJECT (redu, "transform_caps");

  GST_DEBUG_OBJECT (trans,
      "Transforming caps %" GST_PTR_FORMAT " with filter %" GST_PTR_FORMAT " in direction %s", caps, filter,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  othercaps = gst_caps_copy (caps);

  if (filter) {
    GstCaps *intersect;

    intersect = gst_caps_intersect (othercaps, filter);
    gst_caps_unref (othercaps);

    return intersect;
  } else {
    return othercaps;
  }
}

static GstCaps *
gst_hspec_reducer_fixate_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, GstCaps * othercaps)
{
  GstHspecReducer *hspecreducer = GST_HSPEC_REDUCER (trans);
  GstStructure *outs = gst_caps_get_structure(othercaps, 0);
  gint i, j, wavelengths = 0;
  GValue array = { 0 };
  GValue value = { 0 };
  /* reduce the target wavelengths for downstream */

  SpectralInfo inmos;
  init_mosaic(&inmos);
  if (!load_mosaic_from_caps(&inmos, caps)) {
    GST_ERROR("Unable to load mosaic from caps %" GST_PTR_FORMAT, caps);
    return NULL;
  }

  if (direction == GST_PAD_SINK) {
    if (hspecreducer->inclist) {
      /* calculate the wavelengths. scan over input wavelengths and check which ones we keep */
      g_value_init(&array, GST_TYPE_ARRAY);
      for (i=0; i<inmos.size; i++) {
        for (j=0; j<hspecreducer->inclist->len; j++) {
          if (inmos.spectras[i] == g_array_index(hspecreducer->inclist, gint, j)) {
            g_value_init (&value, G_TYPE_INT);
            g_value_set_int (&value, inmos.spectras[i]);
            gst_value_array_append_value (&array, &value);
            g_value_unset (&value);
            wavelengths++;
            break;
          }
        }
      }
      gst_structure_take_value (outs, "wavelength_ids", &array);
      g_value_init (&value, G_TYPE_INT);
      g_value_set_int (&value, wavelengths);
      gst_structure_take_value (outs, "wavelengths", &value);
    }
    else if (hspecreducer->exclist) {
      /* calculate the wavelengths. scan over input wavelengths and check which ones we keep */
      gboolean res;
      g_value_init(&array, GST_TYPE_ARRAY);
      for (i=0; i<inmos.size; i++) {
        res = TRUE;
        for (j=0; j<hspecreducer->exclist->len; j++) {
          if (inmos.spectras[i] == g_array_index(hspecreducer->exclist, gint, j)) {
            res = FALSE;
            break;
          }
        }
        if (res) {
          g_value_init (&value, G_TYPE_INT);
          g_value_set_int (&value, inmos.spectras[i]);
          gst_value_array_append_value (&array, &value);
          g_value_unset (&value);
          wavelengths++;
        }
      }
      gst_structure_take_value (outs, "wavelength_ids", &array);
      g_value_init (&value, G_TYPE_INT);
      g_value_set_int (&value, wavelengths);
      gst_structure_take_value (outs, "wavelengths", &value);
    }
  }
  clear_mosaic(&inmos);
  othercaps = gst_caps_fixate (othercaps);

  GST_DEBUG_OBJECT (trans, "fixated to %" GST_PTR_FORMAT, othercaps);

  return othercaps;
}

static gboolean
gst_hspec_reducer_accept_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps)
{
  GstHspecReducer *hspecreducer = GST_HSPEC_REDUCER (trans);
  gint i, j;
  SpectralInfo mos;
  gboolean res = FALSE;

  GST_DEBUG ("Checking caps acceptibility: %" GST_PTR_FORMAT, caps);
  init_mosaic (&mos);
  if (!load_mosaic_from_caps(&mos, caps)) {
    GST_WARNING("Unable to load mosaic from caps");
    clear_mosaic (&mos);
    return FALSE;
  }

  /* now check if the wavelengths contain the inclusion/exclusion wavelengthids.  */
  if (hspecreducer->inclist) {
    for (j=0; j<hspecreducer->inclist->len; j++){
      res = FALSE;
      for (i=0; i<mos.size; i++) {
        if(mos.spectras[i] == g_array_index(hspecreducer->inclist,
            gint, j)) {
          res = TRUE;
          break;
        }
      }
      if (!res) {
        GST_WARNING("Unable to find wavelength id '%d' in inclusion list",
            g_array_index(hspecreducer->inclist, gint, j));
        break;
      }
    }
  }
  else if (hspecreducer->exclist) {
    for (j=0; j<hspecreducer->exclist->len; j++) {
      res = FALSE;
      for (i=0; i<mos.size; i++) {
        if (mos.spectras[i] == g_array_index(hspecreducer->exclist,
            gint, j)) {
          res = TRUE;
          break;
        }
      }
      if (!res) {
        GST_WARNING("Unable to find wavelength id '%d' in exclusion list",
          g_array_index(hspecreducer->exclist, gint, j));
        break;
      }
    }
  }
  else
    res = TRUE;

  clear_mosaic (&mos);
  return res;
}

static gboolean
gst_hspec_reducer_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstHspecReducer *hspecreducer = GST_HSPEC_REDUCER (trans);
  gint i, j;
  if (!gst_hyperspectral_info_from_caps(&hspecreducer->ininfo, incaps)) {
    GST_ERROR("Unable to retrieve input hyperspectral info from caps %" GST_PTR_FORMAT,
      incaps);
    return FALSE;
  }
  if (!gst_hyperspectral_info_from_caps(&hspecreducer->outinfo, outcaps)) {
    GST_ERROR("Unable to retrieve output hyperspectral info from caps %" GST_PTR_FORMAT,
      outcaps);
    return FALSE;
  }

  hspecreducer->size_set = TRUE;

  if (hspecreducer->wavelength_pos)
    g_array_free (hspecreducer->wavelength_pos, TRUE);
  hspecreducer->wavelength_pos = NULL;
  /* create the list of wavelength positions in a hyperspectral input frame that should
   * be copied over. The order is important!
   */
  if(hspecreducer->inclist) {
    hspecreducer->wavelength_pos = g_array_sized_new (
      FALSE, FALSE, sizeof (gint), hspecreducer->inclist->len);
    for (i=0; i<hspecreducer->ininfo.wavelengths; i++) {
      for (j=0; j<hspecreducer->inclist->len; j++) {
        if (hspecreducer->ininfo.mosaic.spectras[i] == g_array_index(
            hspecreducer->inclist, gint, j))
          g_array_append_val(hspecreducer->wavelength_pos, i);
      }
    }
  }
  else if (hspecreducer->exclist) {
    gboolean res;
    hspecreducer->wavelength_pos = g_array_sized_new (
      FALSE, FALSE, sizeof (gint), hspecreducer->exclist->len);
    for (i=0; i<hspecreducer->ininfo.wavelengths; i++) {
      res = TRUE;
      for (j=0; j<hspecreducer->exclist->len; j++) {
        if (hspecreducer->ininfo.mosaic.spectras[i] == g_array_index(
            hspecreducer->exclist, gint, j)) {
          res = FALSE;
          break;
        }
      }
      if (res)
        g_array_append_val(hspecreducer->wavelength_pos, i);
    }
  }
  /* if no lists are defined, copy over the entire wavelength list */
  else {
    hspecreducer->wavelength_pos = g_array_sized_new (
      FALSE, FALSE, sizeof (gint), hspecreducer->ininfo.wavelengths);
    for (i=0; i<hspecreducer->ininfo.wavelengths; i++) {
      g_array_append_val(hspecreducer->wavelength_pos, i);
    }
  }

  return TRUE;
}

/* transform size */
static gboolean
gst_hspec_reducer_transform_size (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, gsize size, GstCaps * othercaps, gsize * othersize)
{
  GstHspecReducer *hspecreducer = GST_HSPEC_REDUCER (trans);

  if ( direction == GST_PAD_SINK && hspecreducer->size_set) {
    *othersize =  hspecreducer->outinfo.cube_size;
    return TRUE;
  }

  GST_ERROR("Output buffer size has not been set");

  return FALSE;
}

static gboolean
parse_int_list(GString *str, GArray **array)
{
  GArray *tarr = g_array_new (FALSE, FALSE, sizeof (gint));
  long int li;
  char *strptr, *strptr2, *endaddr = &str->str[str->len];
  strptr = str->str;
  do {
    li = strtol(strptr, &strptr2, 10);
    if ((li == LONG_MAX || li == LONG_MIN)&& errno == ERANGE) {
      GST_ERROR("Unable to parse integer in substring: %s", strptr);
      goto fail;
    }
    else if (li < 0 || li > G_MAXINT ) {
      GST_ERROR("Parsed integer is outside of valid range: 0 < %ld < %d",
        li, G_MAXINT);
      goto fail;
    }
    else if (li == 0 ) {
      if (*strptr != '0') {
        GST_ERROR("No valid conversion could be performed for substring %s",
          strptr);
        goto fail;
      }
    }
    g_array_append_val(tarr, li);
    while(strptr2 != endaddr && isspace(*strptr2))
      strptr2++;
    if (strptr2 != endaddr) {
      if(*strptr2 == ',') {
        strptr2++;
      }
      else {
        GST_ERROR ("Unknown character %c in string %s", *strptr2, str->str);
        goto fail;
      }
    }
    strptr = strptr2;
  } while (strptr != endaddr);

  *array = tarr;
  return TRUE;
fail:
  {
    g_array_free(tarr, TRUE);
    return FALSE;
  }
}

/* states */
static gboolean
gst_hspec_reducer_start (GstBaseTransform * trans)
{
  GstHspecReducer *hspecreducer = GST_HSPEC_REDUCER (trans);

  if (hspecreducer->inclist)
    g_array_free (hspecreducer->inclist, TRUE);
  hspecreducer->inclist = NULL;

  if (hspecreducer->exclist)
    g_array_free (hspecreducer->exclist, TRUE);
  hspecreducer->exclist = NULL;

  if (hspecreducer->incstr) {
    if (!parse_int_list(hspecreducer->incstr, &hspecreducer->inclist))
      return FALSE;
  }
  else if (hspecreducer->excstr) {
    if (!parse_int_list(hspecreducer->excstr, &hspecreducer->exclist))
      return FALSE;
  }
  return TRUE;
}

/* transform */
static GstFlowReturn
gst_hspec_reducer_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstHspecReducer *hsred = GST_HSPEC_REDUCER (trans);
  GstHyperspectralFrame inframe, outframe;
  gint i, j, index;

  if (!gst_hyperspectral_frame_map(&inframe, &hsred->ininfo,
    inbuf, GST_MAP_READ)) {
    GST_ERROR_OBJECT (hsred, "Could not map input hyperspectral frame");
    return GST_FLOW_ERROR;
  }

  if (!gst_hyperspectral_frame_map(&outframe, &hsred->outinfo,
    outbuf, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (hsred, "Could not map output hyperspectral frame");
    return GST_FLOW_ERROR;
  }

  if (hsred->ininfo.layout == GST_HSPC_LAYOUT_MULTIPLANE) {
    for (i=0; i<hsred->wavelength_pos->len; i++){
      memcpy(GST_HSPEC_FRAME_WAVELENGTH_DATA_MULTIPLANE(&outframe, i),
             GST_HSPEC_FRAME_WAVELENGTH_DATA_MULTIPLANE(&inframe,
              g_array_index (hsred->wavelength_pos, gint, i)),
             inframe.info.wavelength_size);
    }
  } else if (hsred->ininfo.layout == GST_HSPC_LAYOUT_INTERLEAVED) {
    for (i=0; i<hsred->wavelength_pos->len; i++) {
      index = g_array_index (hsred->wavelength_pos, gint, i);
      for (j=0; j<inframe.info.wavelength_elems; j++) {
        memcpy (outframe.data+(j*outframe.info.wavelengths+i)*inframe.info.bytesize,
                inframe.data+(j*inframe.info.wavelengths+index)*inframe.info.bytesize,
                inframe.info.bytesize);
      }
    }
  } else {
    GST_ERROR("Unhandled spectral layout %d", hsred->ininfo.layout);
    return GST_FLOW_ERROR;
  }

  gst_hyperspectral_frame_unmap(&outframe);
  gst_hyperspectral_frame_unmap(&inframe);
  return GST_FLOW_OK;
}

gboolean
gst_hspec_reducer_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "hspec-reducer", GST_RANK_NONE,
      GST_TYPE_HSPEC_REDUCER);
}
