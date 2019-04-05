@echo off

echo Making silgy_app using Microsoft compiler...

cl silgy_app.cpp ^
..\lib\silgy_eng.c ..\lib\silgy_lib.c ..\lib\silgy_usr.c ^
/EHsc ^
-I . -I ..\lib ^
/Fe..\bin\silgy_app

echo.

echo Make sure you have dirent.h from https://github.com/tronkko/dirent/tree/master/include

echo Remember to set the environment with vcvars32 (path: C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin)
