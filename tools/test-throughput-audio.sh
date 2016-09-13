#!/bin/sh
GST_PLUGIN_PATH=`dirname $0`/../src/ gst-launch-1.0 \
	audiotestsrc ! \
	audio/x-raw,rate=48000,channels=2,format=S16LE ! \
	identity sync=true ! \
	throughput stderr=true ! \
	lamemp3enc ! \
	fakesink silent=TRUE
