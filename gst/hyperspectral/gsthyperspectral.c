/* GStreamer
 * Copyright (C) <2015> Dimitrios Katsaros <patcherwork@gmail.com>
 *
 * gstopencvqtec.c: plugin registering
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsthyperspectralenc.h"
#include "gsthyperspectraldec.h"
#include "gsthspecfilesink.h"
#include "gsthspecreducer.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_hyperspectralenc_plugin_init (plugin))
    return FALSE;
  if (!gst_hyperspectraldec_plugin_init (plugin))
    return FALSE;
  if (!gst_hspec_file_sink_plugin_init (plugin))
    return FALSE;
  if (!gst_hspec_reducer_plugin_init (plugin))
    return FALSE;
  return TRUE;
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "hyperspectral_qtec"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "hyperspectral_qtec"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://qtec.com/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    hyperspectral,
    "GStreamer Hyperspectral Qtec Plugins",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
