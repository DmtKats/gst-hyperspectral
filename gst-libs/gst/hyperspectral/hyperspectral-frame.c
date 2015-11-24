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

#include "hyperspectral-frame.h"
#include <string.h>
gboolean
gst_hyperspectral_frame_map (GstHyperspectralFrame *frame,
    GstHyperspectralInfo *info, GstBuffer *buffer, GstMapFlags flags)
{
  g_return_val_if_fail (frame != NULL, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (buffer), FALSE);

  frame->info = *info;
  if (!gst_buffer_map (buffer, &frame->map, flags))
    goto map_failed;

  /* do some sanity checks */
  if (frame->map.size < info->cube_size)
    goto invalid_size;

  frame->data = frame->map.data;

  frame->buffer = buffer;
  if ((flags & GST_VIDEO_FRAME_MAP_FLAG_NO_REF) == 0)
    gst_buffer_ref (frame->buffer);

  return TRUE;
map_failed:
  {
    GST_ERROR ("failed to map buffer");
    return FALSE;
  }
invalid_size:
  {
    GST_ERROR ("invalid buffer size %" G_GSIZE_FORMAT " < %" G_GSIZE_FORMAT,
        frame->map.size, info->cube_size);
    gst_buffer_unmap (buffer, &frame->map);
    memset (frame, 0, sizeof (GstHyperspectralFrame));
    return FALSE;
  }
}


void
gst_hyperspectral_frame_unmap (GstHyperspectralFrame *frame)
{
  GstMapFlags flags;

  g_return_if_fail (frame != NULL);

  flags = frame->map.flags;
  gst_buffer_unmap (frame->buffer, &frame->map);

  if ((flags & GST_VIDEO_FRAME_MAP_FLAG_NO_REF) == 0)
    gst_buffer_unref (frame->buffer);

}
