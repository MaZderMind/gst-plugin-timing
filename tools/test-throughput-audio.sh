#!/bin/sh
GST_PLUGIN_PATH=`dirname $0`/../src/ gst-launch-1.0 \
	audiotestsrc ! \
	audio/x-raw,rate=48000,channels=2,format=S16LE ! \
	throughput stderr=true interval=500 name=raw ! \
	lamemp3enc ! \
	throughput stderr=true interval=500 name=mp3 ! \
	fakesink silent=TRUE
