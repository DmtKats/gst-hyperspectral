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

#ifndef _GST_HSPEC_FILE_SINK_H_
#define _GST_HSPEC_FILE_SINK_H_

#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <gst/hyperspectral/hyperspectral.h>
#include <stdio.h>

G_BEGIN_DECLS

#define GST_TYPE_HSPEC_FILE_SINK   (gst_hspec_file_sink_get_type())
#define GST_HSPEC_FILE_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HSPEC_FILE_SINK,GstHspecFileSink))
#define GST_HSPEC_FILE_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HSPEC_FILE_SINK,GstHspecFileSinkClass))
#define GST_IS_HSPEC_FILE_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HSPEC_FILE_SINK))
#define GST_IS_HSPEC_FILE_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HSPEC_FILE_SINK))

typedef enum {
  GST_FILE_SINK_RAW,
  GST_FILE_SINK_GERBIL,
  GST_FILE_SINK_CSV,
  GST_FILE_SINK_XML,
} GstFileSinkFileFormat;

typedef struct _GstHspecFileSink GstHspecFileSink;
typedef struct _GstHspecFileSinkClass GstHspecFileSinkClass;

typedef struct {
  gchar * data;
  glong size;
} TBuf;

typedef struct {
  gboolean (*write_file_func) (GstHspecFileSink *sink, GstHyperspectralFrame *frame);
  gboolean (*init_file_func) (GstHspecFileSink *sink);
  gboolean (*cleanup_file_func) (GstHspecFileSink *sink);
} func_set;

struct _GstHspecFileSink
{
  GstVideoSink base_hspecfilesink;

  GstHyperspectralInfo hinfo;

  GString *filepath;
  gint filepath_len;
  GString *filename;
  gint filename_len;
  gint * orderedspectra;
  glong image_counter;

  gchar * tbuff;
  gint tbuffsize;
  TBuf tbuf;

  GstFileSinkFileFormat file_format;

  func_set fset;

  FILE *file;
};

struct _GstHspecFileSinkClass
{
  GstVideoSinkClass base_hspecfilesink_class;
};

GType gst_hspec_file_sink_get_type (void);

gboolean gst_hspec_file_sink_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif
