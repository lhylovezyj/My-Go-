@echo off
rem Adjust the g++ path and EasyX include/lib paths as needed for your system
set GXX=E:\mingw64\bin\g++.exe

%GXX% -g -finput-charset=UTF-8 -fexec-charset=GBK -IE:\code\third_party\easyx\include -LE:\code\third_party\easyx\lib easyx_compat.cpp finalproject_6.0.cpp -leasyx -lcomdlg32 -o finalproject_6.0.exe
if %errorlevel% neq 0 (
    echo Build failed.
    pause
) else (
    echo Build succeeded: finalproject_6.0.exe
)
