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
 * SECTION:element-gsthspecfilesink
 *
 * The hspec-filesink element converts a hyperspectral stream to files,
 * formatted for viewing in file.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v v4l2src ! hspecenc ! hspec-filesink
 * ]|
 * Takes the input of a hyperspectral camera, converts it into a hyperspectral cube
 * and stores it in file file format.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <glib/gstdio.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <byteswap.h>
#include <sys/stat.h>
#include "gsthspecfilesink.h"

GST_DEBUG_CATEGORY_STATIC (gst_hspec_file_sink_debug_category);
#define GST_CAT_DEFAULT gst_hspec_file_sink_debug_category

/* prototypes */


static void gst_hspec_file_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_hspec_file_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_hspec_file_sink_dispose (GObject * object);
static void gst_hspec_file_sink_finalize (GObject * object);
static gboolean gst_hspec_file_sink_set_caps (GstBaseSink * sink, GstCaps * caps);
static gboolean gst_hspec_file_sink_start (GstBaseSink * sink);
static gboolean gst_hspec_file_sink_stop (GstBaseSink * sink);

static GstFlowReturn gst_hspec_file_sink_show_frame (GstVideoSink * video_sink,
    GstBuffer * buf);


/* file writer function definitions */
static gboolean write_raw(GstHspecFileSink * sink, GstHyperspectralFrame * frame);
static gboolean init_raw(GstHspecFileSink * sink);
static gboolean cleanup_raw(GstHspecFileSink * sink);
static gboolean write_gerbil(GstHspecFileSink * sink, GstHyperspectralFrame * frame);
static gboolean write_csv(GstHspecFileSink * sink, GstHyperspectralFrame * frame);
static gboolean write_xml(GstHspecFileSink * sink, GstHyperspectralFrame * frame);

/* file writer helper functions and structs */

#define GST_TYPE_HSPEC_FILE_SINK_FILE_FORMAT (gst_hspec_file_sink_file_format_get_type ())
static GType
gst_hspec_file_sink_file_format_get_type (void)
{
  static GType file_sink_format_type = 0;
  static const GEnumValue formats[] = {
    {GST_FILE_SINK_RAW, "raw", "raw"},
    {GST_FILE_SINK_GERBIL, "gerbil", "Gerbil"},
    {GST_FILE_SINK_CSV, "csv", "CSV"},
    {GST_FILE_SINK_XML, "xml", "XML"},
    {0, NULL, NULL}
  };

  if (!file_sink_format_type) {
    file_sink_format_type =
    g_enum_register_static ("GstHspecFileSinkFileFormats", formats);
  }
  return file_sink_format_type;
}

typedef struct
{
  GstFileSinkFileFormat format;
  gchar category_name[32];
  gchar filename_default[32];
  func_set f_set;
} write_cat_entry;

static const write_cat_entry write_cat_params[] = {
  {GST_FILE_SINK_RAW, "RAW", "raw_hyperspectral_file",
    {write_raw, init_raw, cleanup_raw}},
  {GST_FILE_SINK_GERBIL, "Gerbil", "gerbilsink_image",
    {write_gerbil, NULL, NULL}},
  {GST_FILE_SINK_CSV, "CSV", "csv_image",
    {write_csv, NULL, NULL}},
  {GST_FILE_SINK_XML, "XML", "xml_image",
    {write_xml, NULL, NULL}},
};

#define FORMAT_COUNT (G_N_ELEMENTS (write_cat_params))

static gboolean
set_write_category(GstHspecFileSink *sink, GstFileSinkFileFormat format) {

  if (!(format<FORMAT_COUNT)) {
    GST_WARNING("Unknown format id %d, not setting format", format);
    return FALSE;
  }

  if (sink->filename)
    g_string_free(sink->filename, TRUE);
  sink->filename = g_string_new(write_cat_params[format].filename_default);
  /* these should be the same, however doing this just in case later additions have
   * their order misaligned
   */
  sink->file_format = write_cat_params[format].format;
  return TRUE;
}

gboolean
set_write_funcs (GstHspecFileSink *sink) {

  sink->fset =  write_cat_params[sink->file_format].f_set;
  return TRUE;
}


/* properties */
enum
{
  PROP_0,
  PROP_FILE_NAME,
  PROP_FILE_PATH,
  PROP_FILE_FORMAT
};

enum
{
  SIGNAL_IMAGE_CREATED,
  LAST_SIGNAL
};

#define FILE_PATH_DEFAULT "./\0"

static guint gst_hspec_file_sink_signals[LAST_SIGNAL] = { 0 };

/* pad templates */

static GstStaticPadTemplate gst_hspec_file_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_HYPERSPECTRAL_CAPS_MAKE_WITH_ALL_FORMATS())
    );

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstHspecFileSink, gst_hspec_file_sink, GST_TYPE_VIDEO_SINK,
  GST_DEBUG_CATEGORY_INIT (gst_hspec_file_sink_debug_category, "hspec-filesink", 0,
  "debug category for hspecfilesink element"));

static void
gst_hspec_file_sink_class_init (GstHspecFileSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoSinkClass *video_sink_class = GST_VIDEO_SINK_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&gst_hspec_file_sink_template));


  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Hyperspectral file sink", "hyperspectral/Video",
      "A sink for saving a hyperspectral image into the"
      "file format expected by file",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  gobject_class->set_property = gst_hspec_file_sink_set_property;
  gobject_class->get_property = gst_hspec_file_sink_get_property;
  gobject_class->dispose = gst_hspec_file_sink_dispose;
  gobject_class->finalize = gst_hspec_file_sink_finalize;
  video_sink_class->show_frame = GST_DEBUG_FUNCPTR (gst_hspec_file_sink_show_frame);
  base_sink_class->set_caps = gst_hspec_file_sink_set_caps;
  base_sink_class->start = gst_hspec_file_sink_start;
  base_sink_class->stop = gst_hspec_file_sink_stop;

  /* signal definition */
  gst_hspec_file_sink_signals[SIGNAL_IMAGE_CREATED] = g_signal_new ("file-image-created",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);

  /* property definition */

  g_object_class_install_property (gobject_class, PROP_FILE_NAME,
    g_param_spec_string ("imgname", "ImgName",
        "The name used for the generated images", "",
        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_FILE_PATH,
      g_param_spec_string ("filepath", "FilePath",
        "The path where the images should be stored", FILE_PATH_DEFAULT,
        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_FILE_FORMAT,
      g_param_spec_enum ("format", "file format",
        "The format of the files being generated, will set file path to default for category!",
        GST_TYPE_HSPEC_FILE_SINK_FILE_FORMAT,
        GST_FILE_SINK_RAW, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/* helper functions */

/* snprintf helper functions */

static void
clear_tbuf(TBuf *tb) {
  g_return_if_fail(tb!=NULL);
  memset (tb, 0, sizeof(TBuf));
}

static gboolean
reset_tbuf(TBuf *tb) {
  g_return_val_if_fail(tb!=NULL, FALSE);
  if (tb->data)
    free(tb->data);
  tb->data = malloc(sizeof(gchar) * 10);
  if(!tb->data) {
    GST_ERROR("Unable to allocate buffer of size 10");
    tb->size = 0;
    tb->data = NULL;
    return FALSE;
  }
  tb->size = 10;
  return TRUE;
}

static void
dealloc_tbuf(TBuf *tb) {
  g_return_if_fail(tb!=NULL);
  if(tb->data)
    free(tb->data);
  tb->data = NULL;
  tb->size = 0;
}

static gboolean
realloc_tbuf_if_needed(TBuf *tb, gint bytes) {
  g_return_val_if_fail(tb!=NULL, FALSE);
  gchar *tmp = NULL;
  if(bytes > tb->size) {
    tmp = realloc(tb->data, bytes);
    if (tmp == NULL) {
      GST_ERROR("Error reallocating memory for tbuf");
      return FALSE;
    }
    tb->data = tmp;
    tb->size = bytes;
  }
  return TRUE;
}

/*constructes string reprisenatation of long value*/
static gboolean
glong_to_string(TBuf *tb, glong value) {

  int strsize, res;
  g_return_val_if_fail(tb!=NULL, FALSE);

  /* first, get size of int */
  strsize = snprintf(NULL, 0, "%lu", value);
  if (!(strsize > 0))
    goto snprintferr;

  if (!realloc_tbuf_if_needed(tb, strsize+1))
    return FALSE;

  res = snprintf(tb->data, strsize+1, "%lu",
    value);

  if (!((res>0) && (res<tb->size))) {
    GST_ERROR("Returned string size from snprintf not"
      " of the expected size: %d", res);
    return FALSE;
  }
  return TRUE;

snprintferr:
  {
    GST_ERROR("Encoding error with snprintf");
    return FALSE;
  }
}


static void
gst_hspec_file_sink_init (GstHspecFileSink *sink)
{
  gst_hyperspectral_info_init(&sink->hinfo);
  sink->filepath = g_string_new(FILE_PATH_DEFAULT);
  sink->filepath_len = 0;
  sink->filename = NULL;
  sink->filename_len = 0;
  sink->orderedspectra = NULL;
  sink->image_counter = 0;
  clear_tbuf(&sink->tbuf);
  reset_tbuf(&sink->tbuf);

  set_write_category(sink, GST_FILE_SINK_RAW);

  sink->file = NULL;
}

void
gst_hspec_file_sink_dispose (GObject * object)
{
  GstHspecFileSink *sink = GST_HSPEC_FILE_SINK (object);

  GST_DEBUG_OBJECT (sink, "dispose");

  gst_hyperspectral_info_clear(&sink->hinfo);
  if (sink->filepath)
    g_string_free(sink->filepath, TRUE);
  sink->filepath = g_string_new(FILE_PATH_DEFAULT);
  sink->filepath_len = 0;
  if (sink->orderedspectra)
    free (sink->orderedspectra);
  sink->orderedspectra = NULL;
  sink->image_counter = 0;

  reset_tbuf(&sink->tbuf);
  set_write_category(sink, GST_FILE_SINK_RAW);

  if (sink->file)
    fclose(sink->file);
  sink->file = NULL;

  G_OBJECT_CLASS (gst_hspec_file_sink_parent_class)->dispose (object);
}

void
gst_hspec_file_sink_finalize (GObject * object)
{
  GstHspecFileSink *sink = GST_HSPEC_FILE_SINK (object);

  GST_DEBUG_OBJECT (sink, "finalize");

  gst_hyperspectral_info_clear(&sink->hinfo);

  if (sink->filepath)
    g_string_free(sink->filepath, TRUE);
  sink->filepath = NULL;

  if (sink->filename)
    g_string_free(sink->filename, TRUE);
  sink->filename = NULL;

  if (sink->orderedspectra)
    free (sink->orderedspectra);
  sink->orderedspectra = NULL;

  dealloc_tbuf(&sink->tbuf);

  if (sink->file)
    fclose(sink->file);
  sink->file = NULL;

  G_OBJECT_CLASS (gst_hspec_file_sink_parent_class)->finalize (object);
}

void
gst_hspec_file_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstHspecFileSink *sink = GST_HSPEC_FILE_SINK (object);

  GST_DEBUG_OBJECT (sink, "set_property");

  switch (property_id) {
    case PROP_FILE_NAME:
      if (sink->filename)
        g_string_free(sink->filename, TRUE);
      sink->filename = g_string_new(g_value_get_string(value));
      break;
    case PROP_FILE_PATH:
      if (sink->filepath)
        g_string_free(sink->filepath, TRUE);
      sink->filepath = g_string_new(g_value_get_string(value));
      break;
    case PROP_FILE_FORMAT:
      set_write_category(sink, g_value_get_enum(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_hspec_file_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstHspecFileSink *sink = GST_HSPEC_FILE_SINK (object);

  GST_DEBUG_OBJECT (sink, "get_property");

  switch (property_id) {
    case PROP_FILE_NAME:
      g_value_set_string(value, sink->filename->str);
      break;
    case PROP_FILE_PATH:
      g_value_set_string(value, sink->filepath->str);
      break;
    case PROP_FILE_FORMAT:
      g_value_set_enum(value, sink->file_format);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

int compare_func (const void * a, const void * b)
{
  return ( *(int*)a - *(int*)b );
}

static gboolean
gst_hspec_file_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{

  GstHspecFileSink *fsink = GST_HSPEC_FILE_SINK (sink);
  GST_DEBUG("Caps recieved: %" GST_PTR_FORMAT, caps);

  if (!gst_hyperspectral_info_from_caps(&fsink->hinfo, caps)){
    GST_ERROR("Unable to extract hyperspectral info from caps");
    return FALSE;
  }

  if (!set_write_funcs(fsink)) {
    GST_ERROR("Unable to set write functions");
    return FALSE;
  }

  /* create sorted array of spectras */
  if (fsink->orderedspectra)
    free (fsink->orderedspectra);

  fsink->orderedspectra = malloc (fsink->hinfo.mosaic.size * sizeof(gint));
  memcpy(fsink->orderedspectra, fsink->hinfo.mosaic.spectras,
    fsink->hinfo.mosaic.size * sizeof(gint));

  qsort (fsink->orderedspectra, fsink->hinfo.mosaic.size,
    sizeof(gint), compare_func);

  if (fsink->fset.init_file_func)
    return fsink->fset.init_file_func(fsink);

  return TRUE;

}

static gboolean
gst_hspec_file_sink_start (GstBaseSink * sink) {
  GstHspecFileSink *filesink = GST_HSPEC_FILE_SINK (sink);

  GST_DEBUG("Starting sink");

  if (filesink->filepath->str[filesink->filepath->len-1] != '/') {
    g_string_append_c(filesink->filepath, '/');
  }

  /* test for filepath validity */
  if (access(filesink->filepath->str, F_OK) != 0) {
    GST_ERROR("File path invalid: %s", filesink->filepath->str);
    return FALSE;
  }

  /* store string size before appending */
  filesink->filepath_len = filesink->filepath->len;
  filesink->filename_len = filesink->filename->len;


  return TRUE;
}

static gboolean gst_hspec_file_sink_stop (GstBaseSink * sink) {

  GstHspecFileSink *filesink = GST_HSPEC_FILE_SINK (sink);

  if (filesink->fset.cleanup_file_func)
    return filesink->fset.cleanup_file_func(filesink);

  return TRUE;
}

static gboolean
init_raw(GstHspecFileSink * sink) {

  int i;
  GST_DEBUG("INIT RAW");
  g_string_append(sink->filepath, sink->filename->str);
  if (sink->file)
    fclose(sink->file);

  GST_DEBUG("Opening file: %s", sink->filepath->str);
  if ((sink->file = g_fopen(sink->filepath->str, "w")) == NULL)  {
    GST_ERROR("An error occured while trying to create file %s", sink->filepath->str);
    goto error;
  }

  /* write hspec info at the beginning of file */
  g_fprintf(sink->file, "video/hyperspectral-cube, width=(int)%d, height=(int)%d"
    "wavelengths=(int)%d, format=(string)%s, wavelength_ids=(int)< ",
      sink->hinfo.width, sink->hinfo.height, sink->hinfo.wavelengths,
      gst_video_format_to_string(sink->hinfo.format));

  for (i=0; i<sink->hinfo.wavelengths; i++) {
    g_fprintf(sink->file, "%d", sink->hinfo.mosaic.spectras[i]);
    if (i!= (sink->hinfo.wavelengths-1))
      g_fprintf(sink->file, ", ");
  }
  g_fprintf(sink->file, " >\n");

  return TRUE;

error:
  {
    if(sink->file) {
      fclose(sink->file);
      sink->file = NULL;
    }
    return FALSE;
  }
}

static gboolean
cleanup_raw(GstHspecFileSink * sink) {
  GST_DEBUG("RAW CLEANUP");
  if (sink->file)
    fclose(sink->file);
  sink->file = NULL;
  g_signal_emit (sink, gst_hspec_file_sink_signals[SIGNAL_IMAGE_CREATED],
    0, sink->filepath->str);
  return TRUE;
}

static gboolean
write_raw (GstHspecFileSink * sink, GstHyperspectralFrame * frame) {

  /* write raw data frame to file */
  g_fprintf(sink->file, "<frame_start_%ld>", sink->image_counter);
  fwrite(frame->data, frame->info.bytesize, frame->info.cube_elems, sink->file);
  g_fprintf(sink->file, "<frame_end_%ld>\n", sink->image_counter);

  return TRUE;
}

static gboolean write_csv(GstHspecFileSink * sink, GstHyperspectralFrame * frame) {
  FILE *hspecFile = NULL;
  gint i, j, z;
  GString *filepath =  g_string_truncate(sink->filepath, sink->filepath_len);
  g_string_append(sink->filepath, sink->filename->str);
  union {
    guint8 *u8;
    guint16 *u16;
    gpointer ptr;
  } data;
  data.ptr = frame->data;

  g_string_append(filepath, ".csv");
  GST_DEBUG("Opening file: %s", filepath->str);
  if ((hspecFile = g_fopen(filepath->str, "w")) == NULL)  {
    GST_ERROR("An error occured while trying to create file %s", filepath->str);
    goto error;
  }

  g_fprintf(hspecFile, "#HS-CSV image: "
    "NumFrames: %d FrameWidth: %d FrameHeight: %d Format: %s\n",
      sink->hinfo.width, sink->hinfo.height, sink->hinfo.wavelengths,
      gst_video_format_to_string(sink->hinfo.format));

  if (sink->hinfo.format == GST_VIDEO_FORMAT_GRAY8 &&
      sink->hinfo.layout == GST_HSPC_LAYOUT_MULTIPLANE) {
    for (i=0; i<frame->info.height; i++) {
      for (j=0; j<frame->info.width; j++) {
        for (z=0; z<frame->info.wavelengths; z++) {
          g_fprintf(hspecFile, "%u,", data.u8[j+(frame->info.width)*i+
            ((frame)->info.width)*((frame)->info.height)*z]);
        }
        fputc('\n', hspecFile);
      }
      fputc('\n', hspecFile);
    }
  } else if (sink->hinfo.format == GST_VIDEO_FORMAT_GRAY8 &&
             sink->hinfo.layout == GST_HSPC_LAYOUT_INTERLEAVED) {
    for (i=0; i<frame->info.height; i++) {
      for (j=0; j<frame->info.width; j++) {
        for (z=0; z<frame->info.wavelengths; z++) {
          g_fprintf(hspecFile, "%u,",
            data.u8[(j+(frame->info.width)*i)*frame->info.wavelengths + z]);
        }
        fputc('\n', hspecFile);
      }
      fputc('\n', hspecFile);
    }
  } else if (sink->hinfo.format == GST_VIDEO_FORMAT_GRAY16_LE &&
             sink->hinfo.layout == GST_HSPC_LAYOUT_MULTIPLANE) {
    for (i=0; i<frame->info.height; i++) {
      for (j=0; j<frame->info.width; j++) {
        for (z=0; z<frame->info.wavelengths; z++) {
          g_fprintf(hspecFile, "%u,", __bswap_16(data.u16[j+i*frame->info.width+
            (frame)->info.width*(frame)->info.height*z]));
        }
        fputc('\n', hspecFile);
      }
      fputc('\n', hspecFile);
    }
  } else if (sink->hinfo.format == GST_VIDEO_FORMAT_GRAY16_LE &&
             sink->hinfo.layout == GST_HSPC_LAYOUT_INTERLEAVED) {
    for (i=0; i<frame->info.height; i++) {
      for (j=0; j<frame->info.width; j++) {
        for (z=0; z<frame->info.wavelengths; z++) {
          g_fprintf(hspecFile, "%u,",
            __bswap_16(data.u16[(j+(frame->info.width)*i)*frame->info.wavelengths + z]));
        }
        fputc('\n', hspecFile);
      }
      fputc('\n', hspecFile);
    }
  } else if (sink->hinfo.format == GST_VIDEO_FORMAT_GRAY16_BE &&
             sink->hinfo.layout == GST_HSPC_LAYOUT_MULTIPLANE) {
    for (i=0; i<frame->info.height; i++) {
      for (j=0; j<frame->info.width; j++) {
        for (z=0; z<frame->info.wavelengths; z++) {
          g_fprintf(hspecFile, "%u,",
            data.u16[j+i*frame->info.width+
             (frame)->info.width*(frame)->info.height*z]);
        }
        fputc('\n', hspecFile);
      }
      fputc('\n', hspecFile);
    }
  } else if (sink->hinfo.format == GST_VIDEO_FORMAT_GRAY16_BE &&
             sink->hinfo.layout == GST_HSPC_LAYOUT_INTERLEAVED) {
    for (i=0; i<frame->info.height; i++) {
      for (j=0; j<frame->info.width; j++) {
        for (z=0; z<frame->info.wavelengths; z++) {
          g_fprintf(hspecFile, "%u,",
            data.u16[(j+(frame->info.width)*i)*frame->info.wavelengths + z]);
        }
        fputc('\n', hspecFile);
      }
      fputc('\n', hspecFile);
    }
  } else
    goto layout_error;

  for (i=0; i<sink->hinfo.wavelengths; i++)
    g_fprintf(hspecFile, "%d,%d,\n", i, sink->orderedspectra[i]);

  fclose(hspecFile);
  g_signal_emit (sink, gst_hspec_file_sink_signals[SIGNAL_IMAGE_CREATED],
    0, sink->filepath->str);
  return TRUE;

error:
  {
    if(hspecFile)
      fclose(hspecFile);
    return FALSE;
  }
layout_error:
  {
    if(hspecFile)
      fclose(hspecFile);
    GST_ERROR("Unhandled combination of layout type %d and format %d",
      sink->hinfo.layout, sink->hinfo.format);
    return FALSE;
  }
}

static gboolean write_xml(GstHspecFileSink * sink, GstHyperspectralFrame * frame) {
  FILE *hspecFile = NULL;
  gint i, j, z;
  GString *filepath =  g_string_truncate(sink->filepath, sink->filepath_len);
  g_string_append(sink->filepath, sink->filename->str);

  g_string_append(filepath, ".xml");
  GST_DEBUG("Opening file: %s", filepath->str);
  if ((hspecFile = g_fopen(filepath->str, "w")) == NULL)  {
    GST_ERROR("An error occured while trying to create file %s", filepath->str);
    goto error;
  }
  /* might need to use fwprintf */
  g_fprintf(hspecFile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<hyperspectral_image>"
      "<image_width>%d</image_width>"
      "<image_height>%d</image_height>"
      "<spectral_line_map num_spectral_lines=\"%d\">",
      sink->hinfo.width, sink->hinfo.height, sink->hinfo.wavelengths);

  for (i=0; i<sink->hinfo.wavelengths; i++)
    g_fprintf(hspecFile, "<wavelength>%d</wavelength>", sink->orderedspectra[i]);
  g_fprintf(hspecFile, "</spectral_line_map>");

  if ((frame->info.format == GST_VIDEO_FORMAT_GRAY8 ||
       frame->info.format == GST_VIDEO_FORMAT_GRAY16_LE) &&
      frame->info.layout == GST_HSPC_LAYOUT_MULTIPLANE) {
    for (i=0; i<frame->info.height; i++) {
      g_fprintf(hspecFile, "<frame frame_index=\"%d\">", i);
      for (j=0; j<frame->info.width; j++) {
        g_fprintf(hspecFile, "<pixel index=\"%d\">", j);
        for (z=0; z<frame->info.wavelengths; z++) {
          fwrite(GST_HSPEC_FRAME_WAVELENGTH_DATA_MULTIPLANE(frame, z) +
            j*frame->info.bytesize + i*frame->info.width*frame->info.bytesize,
            frame->info.bytesize, 1, hspecFile);
        }
        g_fprintf(hspecFile, "</pixel>");
      }
      g_fprintf(hspecFile, "</frame>");
    }
  } else if ((frame->info.format == GST_VIDEO_FORMAT_GRAY8 ||
              frame->info.format == GST_VIDEO_FORMAT_GRAY16_LE) &&
             frame->info.layout == GST_HSPC_LAYOUT_INTERLEAVED) {
    for (i=0; i<frame->info.height; i++) {
      g_fprintf(hspecFile, "<frame frame_index=\"%d\">", i);
      for (j=0; j<frame->info.width; j++) {
        g_fprintf(hspecFile, "<pixel index=\"%d\">", j);
        fwrite(frame->data + j*frame->info.bytesize +
          i*frame->info.width*frame->info.bytesize,
          frame->info.bytesize, frame->info.wavelength_elems, hspecFile);
        g_fprintf(hspecFile, "</pixel>");
      }
      g_fprintf(hspecFile, "</frame>");
    }
  }
  else if (frame->info.format == GST_VIDEO_FORMAT_GRAY16_BE &&
      frame->info.layout == GST_HSPC_LAYOUT_MULTIPLANE) {
    guint8 *data;
    for (i=0; i<frame->info.height; i++) {
      g_fprintf(hspecFile, "<frame frame_index=\"%d\">", i);
      for (j=0; j<frame->info.width; j++) {
        g_fprintf(hspecFile, "<pixel index=\"%d\">", j);
        for (z=0; z<frame->info.wavelengths; z++) {
          data = ((guint8*) (GST_HSPEC_FRAME_WAVELENGTH_DATA_MULTIPLANE(frame, z) +
                    j*frame->info.bytesize + i*frame->info.width*frame->info.bytesize));
          fputc(data[1], hspecFile);
          fputc(data[0], hspecFile);
        }
        g_fprintf(hspecFile, "</pixel>");
      }
      g_fprintf(hspecFile, "</frame>");
    }
  } else if (frame->info.format == GST_VIDEO_FORMAT_GRAY16_BE &&
             frame->info.layout == GST_HSPC_LAYOUT_INTERLEAVED) {
    guint8 *data;
    for (i=0; i<frame->info.height; i++) {
      g_fprintf(hspecFile, "<frame frame_index=\"%d\">", i);
      for (j=0; j<frame->info.width; j++) {
        g_fprintf(hspecFile, "<pixel index=\"%d\">", j);
        for (z=0; z<frame->info.wavelengths; z++) {
          data = ((guint8*) (((j + i*frame->info.width)*frame->info.wavelengths
            + z)*frame->info.bytesize));
          fputc(data[1], hspecFile);
          fputc(data[0], hspecFile);
        }
        g_fprintf(hspecFile, "</pixel>");
      }
      g_fprintf(hspecFile, "</frame>");
    }
  }
  else {

  }
  g_fprintf(hspecFile, "</hyperspectral_image>");

  fclose(hspecFile);

  g_signal_emit (sink, gst_hspec_file_sink_signals[SIGNAL_IMAGE_CREATED],
    0, sink->filepath->str);

  return TRUE;

error:
  {
    if(hspecFile)
      fclose(hspecFile);
    return FALSE;
  }
}

static gboolean
write_gerbil(GstHspecFileSink * sink, GstHyperspectralFrame * frame) {
  FILE *imgFile = NULL, *headerFile = NULL;
  gint i, j, val, len, maxval;
  union {
    guint8 *u8;
    guint16 *u16;
    gpointer ptr;
  } data;
  GstVideoFormat fmt = frame->info.format;
  GString *filepath =  g_string_truncate(sink->filepath, sink->filepath_len);
  g_string_append(sink->filepath, sink->filename->str);

  if (fmt == GST_VIDEO_FORMAT_GRAY8)
    maxval = 255;
  else if (fmt == GST_VIDEO_FORMAT_GRAY16_LE ||
           fmt == GST_VIDEO_FORMAT_GRAY16_BE)
    maxval = 65535;
  else {
    GST_ERROR("Unknown data format");
    return FALSE;
  }

  if (access(filepath->str, F_OK) == -1) {
    if (mkdir(filepath->str, 0700) == -1) {
      GST_ERROR("An error occured while trying to create dir %s", filepath->str);
      GST_ERROR("Reason: %s", g_strerror (errno));
      goto error;
    }
  }
  else {
    GST_WARNING("File %s already exists, passing", filepath->str);
    return TRUE;
  }
  len = filepath->len;
  g_string_append(filepath, ".gerbil");
  GST_DEBUG("Opening file: %s", filepath->str);
  if ((headerFile = g_fopen(filepath->str, "w")) == NULL)  {
    GST_ERROR("An error occured while trying to create file %s", filepath->str);
    goto error;
  }
  g_string_truncate(filepath, len);
  g_string_append_c(filepath, '/');
  g_string_append_c(sink->filename, '/');
  g_fprintf(headerFile, "%d %s\n", sink->hinfo.wavelengths, sink->filename->str);

  for (i=0; i<sink->hinfo.wavelengths; i++) {
    g_string_truncate(filepath, len+1);
    val = sink->hinfo.mosaic.spectras[i];
    glong_to_string(&sink->tbuf, val);
    g_string_append(filepath, sink->tbuf.data);
    g_string_append(filepath, ".pgm");
    g_fprintf(headerFile, "%d.pgm %d\n", sink->orderedspectra[i],
      sink->orderedspectra[i]);
    GST_INFO("filename: %s", filepath->str);

    if ((imgFile = g_fopen(filepath->str, "wb")) == NULL)  {
      GST_ERROR("An error occured while trying to create file %s", filepath->str);
      goto error;
    }
    g_fprintf(imgFile, "P5 %d %d %d ", sink->hinfo.width, sink->hinfo.height, maxval);
    if (frame->info.layout == GST_HSPC_LAYOUT_MULTIPLANE) {
      data.ptr = GST_HSPEC_FRAME_WAVELENGTH_DATA_MULTIPLANE(frame, i);

      for (j=0; j<sink->hinfo.wavelength_elems; j++) {
        if (frame->info.format == GST_VIDEO_FORMAT_GRAY8)
          fputc(data.u8[j], imgFile);
        else if (frame->info.format == GST_VIDEO_FORMAT_GRAY16_LE) {
          fputc(data.u8[j*2+1], imgFile);
          fputc(data.u8[j*2], imgFile);
        }
        else if (frame->info.format == GST_VIDEO_FORMAT_GRAY16_BE) {
          fputc(data.u8[j*2], imgFile);
          fputc(data.u8[j*2+1], imgFile);
        }
        else {
          GST_ERROR("Unknown data format when writing to file");
          return FALSE;
        }
      }
    } else if (frame->info.layout == GST_HSPC_LAYOUT_INTERLEAVED) {
      data.ptr = frame->data + i*sink->hinfo.bytesize;

      for (j=0; j<sink->hinfo.wavelength_elems; j++) {
        if (frame->info.format == GST_VIDEO_FORMAT_GRAY8)
          fputc(data.u8[j*sink->hinfo.wavelengths], imgFile);
        else if (frame->info.format == GST_VIDEO_FORMAT_GRAY16_LE) {
          fputc(data.u8[j*sink->hinfo.wavelengths*2+1], imgFile);
          fputc(data.u8[j*sink->hinfo.wavelengths*2], imgFile);
        }
        else if (frame->info.format == GST_VIDEO_FORMAT_GRAY16_BE) {
          fputc(data.u8[j*sink->hinfo.wavelengths*2], imgFile);
          fputc(data.u8[j*sink->hinfo.wavelengths*2+1], imgFile);
        }
        else {
          GST_ERROR("Unknown data format when writing to file");
          return FALSE;
        }
      }
    } else {
      GST_ERROR("Unhandled cube layout type %d", frame->info.layout);
    }

    fclose(imgFile);
  }

  fclose(headerFile);
  g_string_truncate(filepath, len);
  g_string_append(filepath, ".gerbil");

  g_signal_emit (sink, gst_hspec_file_sink_signals[SIGNAL_IMAGE_CREATED],
    0, sink->filepath->str);
  return TRUE;
error:
  {
    if(headerFile)
      fclose(headerFile);
    if(imgFile)
      fclose(imgFile);
    return FALSE;
  }
}

static GstFlowReturn
gst_hspec_file_sink_show_frame (GstVideoSink * sink, GstBuffer * buf)
{
  GstHspecFileSink *fsink = GST_HSPEC_FILE_SINK (sink);
  GstHyperspectralFrame frame;
  gboolean res = TRUE;

  /* build file name */
  if(!glong_to_string(&fsink->tbuf, fsink->image_counter))
    return GST_FLOW_ERROR;

  g_string_truncate(fsink->filename, fsink->filename_len);
  g_string_append(fsink->filename, fsink->tbuf.data);

  if (!gst_hyperspectral_frame_map(&frame, &fsink->hinfo, buf, GST_MAP_READ))
    goto invalid_buffer;

  if (!fsink->fset.write_file_func) {
    GST_ERROR("Write File function not defined!");
    return GST_FLOW_ERROR;
  }
  res = fsink->fset.write_file_func (fsink, &frame);

  if (fsink->image_counter == G_MAXLONG)
    fsink->image_counter = 0;
  else
    fsink->image_counter++;

  gst_hyperspectral_frame_unmap(&frame);

  if (!res) {
    GST_ERROR("Write function failed for category %s",
      write_cat_params[fsink->file_format].category_name);
    return GST_FLOW_ERROR;
  }
  return GST_FLOW_OK;

invalid_buffer:
  {
    GST_ERROR_OBJECT(sink, "Could not map correctly hyperspectral image");
    return GST_FLOW_ERROR;
  }
}

gboolean
gst_hspec_file_sink_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "hspec-filesink", GST_RANK_NONE,
      GST_TYPE_HSPEC_FILE_SINK);
}
