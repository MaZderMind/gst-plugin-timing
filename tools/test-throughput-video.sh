#!/bin/sh
GST_PLUGIN_PATH=`dirname $0`/../src/ gst-launch-1.0 \
	videotestsrc ! \
	video/x-raw,width=1920,height=1080,framerate=25/1,format=I420 ! \
	throughput stderr=true interval=500 ! \
	x264enc ! \
	fakesink silent=TRUE
