/* gst-plugin-timing
 *             DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *                     Version 2, December 2004
 *
 *  Copyright (C) 2004 Sam Hocevar
 *   14 rue de Plaisance, 75014 Paris, France
 *  Everyone is permitted to copy and distribute verbatim or modified
 *  copies of this license document, and changing it is allowed as long
 *  as the name is changed.
 *
 *             DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *    TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 *   0. You just DO WHAT THE FUCK YOU WANT TO.
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
typedef struct _GstThroughputMeasurement GstThroughputMeasurement;


struct _GstThroughputMeasurement {
  GstClockTime   timestamp;

  guint64        count_offsets;
  guint64        count_buffers;
  guint64        count_bytes;
};
/**
 * GstThroughput:
 *
 * Opaque #GstThroughput data structure
 */
struct _GstThroughput {
  GstBaseTransform   element;

  /*< private >*/
  GstClockID     clock_id;
  gboolean       sync;
  gboolean       stderr;
  guint          interval;
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

  GstCaps        *video_caps;
  GstCaps        *audio_caps;

  GstThroughputMeasurement measurement;
  GstThroughputMeasurement last_measurement;
};

struct _GstThroughputClass {
  GstBaseTransformClass parent_class;
};

G_GNUC_INTERNAL GType gst_throughput_get_type (void);

G_END_DECLS

#endif /* __GST_THROUGHPUT_H__ */
