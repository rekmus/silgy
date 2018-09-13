@echo off

echo Making silgy_app...

g++ silgy_app.cpp ^
..\lib\silgy_eng.c ..\lib\silgy_lib.c ^
-s -O3 ^
-I . -I ..\lib ^
-lws2_32 -lpsapi ^
-o ..\bin\silgy_app ^
-static
