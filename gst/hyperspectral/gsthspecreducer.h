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

#ifndef _GST_HSPEC_REDUCER_H_
#define _GST_HSPEC_REDUCER_H_

#include <gst/base/gstbasetransform.h>
#include <gst/hyperspectral/hyperspectral.h>

G_BEGIN_DECLS

#define GST_TYPE_HSPEC_REDUCER   (gst_hspec_reducer_get_type())
#define GST_HSPEC_REDUCER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HSPEC_REDUCER,GstHspecReducer))
#define GST_HSPEC_REDUCER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HSPEC_REDUCER,GstHspecReducerClass))
#define GST_IS_HSPEC_REDUCER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HSPEC_REDUCER))
#define GST_IS_HSPEC_REDUCER_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HSPEC_REDUCER))

typedef struct _GstHspecReducer GstHspecReducer;
typedef struct _GstHspecReducerClass GstHspecReducerClass;

struct _GstHspecReducer
{
  GstBaseTransform base_hspecreducer;

  GstHyperspectralInfo ininfo;
  GstHyperspectralInfo outinfo;

  GString *incstr;
  GString *excstr;

  GArray *exclist;
  GArray *inclist;

  /* list of input wavelength positions that should be copied */
  GArray *wavelength_pos;

  /* flag to ensure that transform_size calls dont happen before setting the size */
  gboolean size_set;
};

struct _GstHspecReducerClass
{
  GstBaseTransformClass base_hspecreducer_class;
};

GType gst_hspec_reducer_get_type (void);

gboolean gst_hspec_reducer_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif
