@echo off

echo Making silgy_app...

g++ silgy_app.cpp ^
..\lib\silgy_eng.c ..\lib\silgy_lib.c ../lib/silgy_usr.c ^
-s -O3 ^
-I . -I ..\lib ^
-lws2_32 -lpsapi ^
-o ..\bin\silgy_app ^
-static
