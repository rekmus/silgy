@echo off
g++ silgy_app.cpp silgy_eng.c silgy_lib.c -lws2_32 -lpsapi -liconv -o silgy_app
