#!/bin/sh

. ./dev.env

g++ silgy_app.cpp silgy_eng.o silgy_lib.o $WEB_INCLUDE_PATH $WEB_CFLAGS -o $SILGYDIR/bin/silgy_app
