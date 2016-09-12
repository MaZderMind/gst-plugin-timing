/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstthroughput.h:
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


#ifndef __GST_THROUGHPUT_H__
#define __GST_THROUGHPUT_H__


#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS


#define GST_TYPE_THROUGHPUT \
  (gst_throughput_get_type())
#define GST_THROUGHPUT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_THROUGHPUT,GstThroughput))
#define GST_THROUGHPUT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_THROUGHPUT,GstThroughputClass))
#define GST_IS_THROUGHPUT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_THROUGHPUT))
#define GST_IS_THROUGHPUT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_THROUGHPUT))

typedef struct _GstThroughput GstThroughput;
typedef struct _GstThroughputClass GstThroughputClass;

/**
 * GstThroughput:
 *
 * Opaque #GstThroughput data structure
 */
struct _GstThroughput {
  GstBaseTransform 	 element;

  /*< private >*/
  GstClockID     clock_id;
  gboolean       sync;
  gboolean       stderr;
  GstBufferFlags drop_buffer_flags;
  GstClockTime   prev_timestamp;
  GstClockTime   prev_duration;
  guint64        prev_offset;
  guint64        prev_offset_end;
  gchar          *last_message;
  guint64        offset;
  gboolean       signal_handoffs;
  GstClockTime   upstream_latency;
  GCond          blocked_cond;
  gboolean       blocked;
};

struct _GstThroughputClass {
  GstBaseTransformClass parent_class;
};

G_GNUC_INTERNAL GType gst_throughput_get_type (void);

G_END_DECLS

#endif /* __GST_THROUGHPUT_H__ */
