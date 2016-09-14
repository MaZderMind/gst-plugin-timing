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

/**
 * SECTION:element-throughput
 *
 * Dummy element that passes incoming data through unmodified. It has some
 * useful diagnostic functions, such as offset and timestamp checking.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "gstthroughput.h"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_throughput_debug);
#define GST_CAT_DEFAULT gst_throughput_debug

/* Throughput signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_SYNC                    FALSE
#define DEFAULT_STDERR                  FALSE
#define DEFAULT_INTERVAL                1000

enum
{
  PROP_0,
  PROP_LAST_MESSAGE,
  PROP_SYNC,
  PROP_STDERR,
  PROP_INTERVAL
};


#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_throughput_debug, "throughput", 0, "throughput element");
#define gst_throughput_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstThroughput, gst_throughput, GST_TYPE_BASE_TRANSFORM,
    _do_init);

static void gst_throughput_finalize (GObject * object);
static void gst_throughput_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_throughput_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_throughput_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static GstFlowReturn gst_throughput_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean gst_throughput_start (GstBaseTransform * trans);
static gboolean gst_throughput_stop (GstBaseTransform * trans);
static GstStateChangeReturn gst_throughput_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_throughput_accept_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps);
static gboolean gst_throughput_query (GstBaseTransform * base,
    GstPadDirection direction, GstQuery * query);

static GParamSpec *pspec_last_message = NULL;

static void
gst_throughput_finalize (GObject * object)
{
  GstThroughput *throughput;

  throughput = GST_THROUGHPUT (object);

  gst_caps_unref(throughput->video_caps);
  gst_caps_unref(throughput->audio_caps);
  g_free (throughput->last_message);
  g_cond_clear (&throughput->blocked_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_throughput_class_init (GstThroughputClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetrans_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasetrans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_throughput_set_property;
  gobject_class->get_property = gst_throughput_get_property;

  pspec_last_message = g_param_spec_string ("last-message", "last-message",
      "last-message", NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_LAST_MESSAGE,
      pspec_last_message);
  g_object_class_install_property (gobject_class, PROP_SYNC,
      g_param_spec_boolean ("sync", "Synchronize",
          "Synchronize to pipeline clock", DEFAULT_SYNC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_STDERR,
      g_param_spec_boolean ("stderr", "stderr",
          "Also print measurements to stderr", DEFAULT_STDERR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INTERVAL,
      g_param_spec_uint ("interval", "Report-Interval",
          "Interval in Milliseconds between two measurements", 1, G_MAXUINT, DEFAULT_INTERVAL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  gobject_class->finalize = gst_throughput_finalize;

  gst_element_class_set_static_metadata (gstelement_class,
      "Throughput Measurement",
      "Generic",
      "Pass data without modification", "Erik Walthinsen <omega@cse.ogi.edu>");
  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);
  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_throughput_change_state);

  gstbasetrans_class->sink_event = GST_DEBUG_FUNCPTR (gst_throughput_sink_event);
  gstbasetrans_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_throughput_transform_ip);
  gstbasetrans_class->start = GST_DEBUG_FUNCPTR (gst_throughput_start);
  gstbasetrans_class->stop = GST_DEBUG_FUNCPTR (gst_throughput_stop);
  gstbasetrans_class->accept_caps =
      GST_DEBUG_FUNCPTR (gst_throughput_accept_caps);
  gstbasetrans_class->query = gst_throughput_query;
}

static void
gst_throughput_init (GstThroughput * throughput)
{
  throughput->sync = DEFAULT_SYNC;
  throughput->stderr = DEFAULT_STDERR;
  throughput->interval = DEFAULT_INTERVAL;
  throughput->last_message = NULL;

  throughput->video_caps = gst_caps_new_empty_simple("video/x-raw");
  throughput->audio_caps = gst_caps_new_empty_simple("audio/x-raw");

  throughput->measurement.timestamp = GST_CLOCK_TIME_NONE;
  throughput->measurement.count_offsets = 0;
  throughput->measurement.count_buffers = 0;
  throughput->measurement.count_bytes = 0;
  throughput->last_measurement = throughput->measurement;

  g_cond_init (&throughput->blocked_cond);

  gst_base_transform_set_gap_aware (GST_BASE_TRANSFORM_CAST (throughput), TRUE);
}

static void
gst_throughput_notify_last_message (GstThroughput * throughput)
{
  g_object_notify_by_pspec ((GObject *) throughput, pspec_last_message);
  if(throughput->stderr)
    g_message("(%s) %s", GST_ELEMENT_NAME(throughput), throughput->last_message);
}

static GstFlowReturn
gst_throughput_do_sync (GstThroughput * throughput, GstClockTime running_time)
{
  GstFlowReturn ret = GST_FLOW_OK;

  if (throughput->sync &&
      GST_BASE_TRANSFORM_CAST (throughput)->segment.format == GST_FORMAT_TIME) {
    GstClock *clock;

    GST_OBJECT_LOCK (throughput);

    while (throughput->blocked)
      g_cond_wait (&throughput->blocked_cond, GST_OBJECT_GET_LOCK (throughput));


    if ((clock = GST_ELEMENT (throughput)->clock)) {
      GstClockReturn cret;
      GstClockTime timestamp;

      timestamp = running_time + GST_ELEMENT (throughput)->base_time +
          throughput->upstream_latency;

      /* save id if we need to unlock */
      throughput->clock_id = gst_clock_new_single_shot_id (clock, timestamp);
      GST_OBJECT_UNLOCK (throughput);

      cret = gst_clock_id_wait (throughput->clock_id, NULL);

      GST_OBJECT_LOCK (throughput);
      if (throughput->clock_id) {
        gst_clock_id_unref (throughput->clock_id);
        throughput->clock_id = NULL;
      }
      if (cret == GST_CLOCK_UNSCHEDULED)
        ret = GST_FLOW_EOS;
    }
    GST_OBJECT_UNLOCK (throughput);
  }

  return ret;
}

static gboolean
gst_throughput_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstThroughput *throughput;
  gboolean ret = TRUE;

  throughput = GST_THROUGHPUT (trans);

  if (FALSE) {
    const GstStructure *s;
    const gchar *tstr;
    gchar *sstr;

    GST_OBJECT_LOCK (throughput);
    g_free (throughput->last_message);

    tstr = gst_event_type_get_name (GST_EVENT_TYPE (event));
    if ((s = gst_event_get_structure (event)))
      sstr = gst_structure_to_string (s);
    else
      sstr = g_strdup ("");

    throughput->last_message =
        g_strdup_printf ("event (%s:%s) type: %s (%d), %s %p",
        GST_DEBUG_PAD_NAME (trans->sinkpad), tstr, GST_EVENT_TYPE (event),
        sstr, event);
    g_free (sstr);
    GST_OBJECT_UNLOCK (throughput);

    gst_throughput_notify_last_message (throughput);
  }

  if (GST_EVENT_TYPE (event) == GST_EVENT_GAP &&
      trans->have_segment && trans->segment.format == GST_FORMAT_TIME) {
    GstClockTime start, dur;

    gst_event_parse_gap (event, &start, &dur);
    if (GST_CLOCK_TIME_IS_VALID (start)) {
      start = gst_segment_to_running_time (&trans->segment,
          GST_FORMAT_TIME, start);

      gst_throughput_do_sync (throughput, start);
    }
  }

  /* Reset previous timestamp, duration and offsets on SEGMENT
   * to prevent false warnings when checking for perfect streams */
  if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
    throughput->prev_timestamp = throughput->prev_duration = GST_CLOCK_TIME_NONE;
    throughput->prev_offset = throughput->prev_offset_end = GST_BUFFER_OFFSET_NONE;
  }

  return ret;
}

static void
gst_throughput_update_last_message_for_buffer (GstThroughput * throughput,
    GstBuffer * buf, gsize size, guint64 offset_delta)
{
  GST_OBJECT_LOCK (throughput);

  GstClock *sysclock = gst_system_clock_obtain();
  GstClockTime timestamp = gst_clock_get_time(sysclock);
  gst_object_unref(sysclock);

  GstCaps *caps = gst_pad_get_current_caps (GST_BASE_TRANSFORM_SINK_PAD(throughput));
  GstStructure * struc = gst_caps_get_structure(caps, 0);
  gboolean is_video = gst_caps_is_always_compatible(caps, throughput->video_caps);
  gboolean is_audio = gst_caps_is_always_compatible(caps, throughput->audio_caps);

  float offsets_per_second_from_caps = 0.0;
  if(is_video)
  {
    gint num, denom;
    if(gst_structure_get_fraction(struc, "framerate", &num, &denom))
    {
      if(num > 0 && denom > 0)
        offsets_per_second_from_caps = num / denom;
    }
  }
  else if(is_audio)
  {
    int rate;
    if(gst_structure_get_int(struc, "rate", &rate))
    {
      offsets_per_second_from_caps = rate;
    }
  }
  gst_caps_unref(caps);

  //g_message("update throughput->measurement.timestamp = timestamp");
  throughput->measurement.timestamp = timestamp;
  throughput->measurement.count_buffers++;
  throughput->measurement.count_bytes += size;
  if(is_video || is_audio)
    throughput->measurement.count_offsets += offset_delta;


  gboolean new_message = FALSE;
  if(throughput->last_measurement.timestamp == GST_CLOCK_TIME_NONE)
  {
    // first measurement
    //g_message("initialize throughput->last_measurement.timestamp = timestamp");
    throughput->last_measurement.timestamp = timestamp;
  }

  else
  {
    GstClockTime tdelta = throughput->measurement.timestamp - throughput->last_measurement.timestamp;
    //g_message("tdelta = throughput->measurement.timestamp - throughput->last_measurement.timestamp = %" G_GUINT64_FORMAT, tdelta);

    if(tdelta > throughput->interval*1000*1000)
    {
      float f = 1000000000.0/(float)tdelta;
      float buffers_per_second = f * (throughput->measurement.count_buffers - throughput->last_measurement.count_buffers);
      float mbytes_per_second =  f * (throughput->measurement.count_bytes - throughput->last_measurement.count_bytes) / 1024 / 1024;
      float mbits_per_second =   mbytes_per_second * 8;
      float offsets_per_second = f * (throughput->measurement.count_offsets - throughput->last_measurement.count_offsets);

      new_message = TRUE;
      g_free (throughput->last_message);
      if(is_video) {
         throughput->last_message = g_strdup_printf (
          "Transfering %.0f Buffers/s (%.0f Frames/s) of a %.0f Frames/s stream (=%.1f%%) at %.2f MBit/s (= %.2f MByte/s)",
          buffers_per_second,
          offsets_per_second,
          offsets_per_second_from_caps,
          offsets_per_second / offsets_per_second_from_caps * 100,
          mbits_per_second,
          mbytes_per_second
        );
      }
      else if(is_audio)
      {
         throughput->last_message = g_strdup_printf (
          "Transfering %.0f Buffers/s (%.0f Sample/s) of a %.0f Sample/s stream (=%.1f%%) at %.2f MBit/s (= %.2f MByte/s)",
          buffers_per_second,
          offsets_per_second,
          offsets_per_second_from_caps,
          offsets_per_second / offsets_per_second_from_caps * 100,
          mbits_per_second,
          mbytes_per_second
        );
      }
      else
      {
         throughput->last_message = g_strdup_printf (
          "Transfering %.0f Buffers/s at %.2f MBit/s (= %.2f MByte/s)",
          buffers_per_second,
          mbits_per_second,
          mbytes_per_second
        );
      }

      throughput->last_measurement = throughput->measurement;
    }
  }

  GST_OBJECT_UNLOCK (throughput);

  if(G_UNLIKELY(new_message))
    gst_throughput_notify_last_message (throughput);
}

static GstFlowReturn
gst_throughput_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstThroughput *throughput = GST_THROUGHPUT (trans);
  GstClockTime rundts = GST_CLOCK_TIME_NONE;
  GstClockTime runpts = GST_CLOCK_TIME_NONE;
  GstClockTime runtimestamp;
  gsize size;

  size = gst_buffer_get_size (buf);
  guint64 offset_delta = GST_BUFFER_OFFSET (buf) - throughput->prev_offset;

  /* update prev values */
  throughput->prev_timestamp = GST_BUFFER_TIMESTAMP (buf);
  throughput->prev_duration = GST_BUFFER_DURATION (buf);
  throughput->prev_offset_end = GST_BUFFER_OFFSET_END (buf);
  throughput->prev_offset = GST_BUFFER_OFFSET (buf);

  gst_throughput_update_last_message_for_buffer (throughput, buf, size, offset_delta);

  if (trans->segment.format == GST_FORMAT_TIME) {
    rundts = gst_segment_to_running_time (&trans->segment,
        GST_FORMAT_TIME, GST_BUFFER_DTS (buf));
    runpts = gst_segment_to_running_time (&trans->segment,
        GST_FORMAT_TIME, GST_BUFFER_PTS (buf));
  }

  if (GST_CLOCK_TIME_IS_VALID (rundts))
    runtimestamp = rundts;
  else if (GST_CLOCK_TIME_IS_VALID (runpts))
    runtimestamp = runpts;
  else
    runtimestamp = 0;
  ret = gst_throughput_do_sync (throughput, runtimestamp);

  throughput->offset += size;

  return ret;
}

static void
gst_throughput_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstThroughput *throughput;

  throughput = GST_THROUGHPUT (object);

  switch (prop_id) {
    case PROP_SYNC:
      throughput->sync = g_value_get_boolean (value);
      break;
    case PROP_STDERR:
      throughput->stderr = g_value_get_boolean (value);
      break;
    case PROP_INTERVAL:
      throughput->interval = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (throughput), TRUE);
}

static void
gst_throughput_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstThroughput *throughput;

  throughput = GST_THROUGHPUT (object);

  switch (prop_id) {
    case PROP_LAST_MESSAGE:
      GST_OBJECT_LOCK (throughput);
      g_value_set_string (value, throughput->last_message);
      GST_OBJECT_UNLOCK (throughput);
      break;
    case PROP_SYNC:
      g_value_set_boolean (value, throughput->sync);
      break;
    case PROP_STDERR:
      g_value_set_boolean (value, throughput->stderr);
      break;
    case PROP_INTERVAL:
      g_value_set_uint (value, throughput->interval);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_throughput_start (GstBaseTransform * trans)
{
  GstThroughput *throughput;

  throughput = GST_THROUGHPUT (trans);

  throughput->offset = 0;
  throughput->prev_timestamp = GST_CLOCK_TIME_NONE;
  throughput->prev_duration = GST_CLOCK_TIME_NONE;
  throughput->prev_offset_end = GST_BUFFER_OFFSET_NONE;
  throughput->prev_offset = GST_BUFFER_OFFSET_NONE;

  return TRUE;
}

static gboolean
gst_throughput_stop (GstBaseTransform * trans)
{
  GstThroughput *throughput;

  throughput = GST_THROUGHPUT (trans);

  GST_OBJECT_LOCK (throughput);
  g_free (throughput->last_message);
  throughput->last_message = NULL;
  GST_OBJECT_UNLOCK (throughput);

  return TRUE;
}

static gboolean
gst_throughput_accept_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps)
{
  gboolean ret;
  GstPad *pad;

  /* Proxy accept-caps */

  if (direction == GST_PAD_SRC)
    pad = GST_BASE_TRANSFORM_SINK_PAD (base);
  else
    pad = GST_BASE_TRANSFORM_SRC_PAD (base);

  ret = gst_pad_peer_query_accept_caps (pad, caps);

  return ret;
}

static gboolean
gst_throughput_query (GstBaseTransform * base, GstPadDirection direction,
    GstQuery * query)
{
  GstThroughput *throughput;
  gboolean ret;

  throughput = GST_THROUGHPUT (base);

  ret = GST_BASE_TRANSFORM_CLASS (parent_class)->query (base, direction, query);

  if (GST_QUERY_TYPE (query) == GST_QUERY_LATENCY) {
    gboolean live = FALSE;
    GstClockTime min = 0, max = 0;

    if (ret) {
      gst_query_parse_latency (query, &live, &min, &max);

      if (throughput->sync && max < min) {
        GST_ELEMENT_WARNING (base, CORE, CLOCK, (NULL),
            ("Impossible to configure latency before throughput sync=true:"
                " max %" GST_TIME_FORMAT " < min %"
                GST_TIME_FORMAT ". Add queues or other buffering elements.",
                GST_TIME_ARGS (max), GST_TIME_ARGS (min)));
      }
    }

    /* Ignore the upstream latency if it is not live */
    GST_OBJECT_LOCK (throughput);
    if (live)
      throughput->upstream_latency = min;
    else
      throughput->upstream_latency = 0;
    GST_OBJECT_UNLOCK (throughput);

    gst_query_set_latency (query, live || throughput->sync, min, max);
    ret = TRUE;
  }
  return ret;
}

static GstStateChangeReturn
gst_throughput_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstThroughput *throughput = GST_THROUGHPUT (element);
  gboolean no_preroll = FALSE;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_OBJECT_LOCK (throughput);
      throughput->blocked = TRUE;
      GST_OBJECT_UNLOCK (throughput);
      if (throughput->sync)
        no_preroll = TRUE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_OBJECT_LOCK (throughput);
      throughput->blocked = FALSE;
      g_cond_broadcast (&throughput->blocked_cond);
      GST_OBJECT_UNLOCK (throughput);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_OBJECT_LOCK (throughput);
      if (throughput->clock_id) {
        GST_DEBUG_OBJECT (throughput, "unlock clock wait");
        gst_clock_id_unschedule (throughput->clock_id);
      }
      throughput->blocked = FALSE;
      g_cond_broadcast (&throughput->blocked_cond);
      GST_OBJECT_UNLOCK (throughput);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      GST_OBJECT_LOCK (throughput);
      throughput->upstream_latency = 0;
      throughput->blocked = TRUE;
      GST_OBJECT_UNLOCK (throughput);
      if (throughput->sync)
        no_preroll = TRUE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  if (no_preroll && ret == GST_STATE_CHANGE_SUCCESS)
    ret = GST_STATE_CHANGE_NO_PREROLL;

  return ret;
}
