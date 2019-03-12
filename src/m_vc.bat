@echo off

echo Making silgy_app using Microsoft compiler...

cl silgy_app.cpp ^
..\lib\silgy_eng.c ..\lib\silgy_lib.c ^
/EHsc ^
-I . -I ..\lib ^
/Fe..\bin\silgy_app

echo Make sure you have dirent.h from https://github.com/tronkko/dirent/tree/master/include
