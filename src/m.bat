@echo off

echo Making silgy_app...

g++ silgy_app.cpp silgy_eng.c silgy_lib.c ^
-s -O3 ^
-D MEM_SMALL ^
-lws2_32 -lpsapi -liconv ^
-o silgy_app ^
-static
