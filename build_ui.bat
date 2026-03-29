@echo off
set MSVC=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207
set WINSDK=C:\Program Files (x86)\Windows Kits\10
set SDKVER=10.0.26100.0
set VS=C:\Program Files\Microsoft Visual Studio\2022\Community
set QT=C:\Qt\6.8.3\msvc2022_64
set PATH=%MSVC%\bin\Hostx64\x64;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%WINSDK%\bin\%SDKVER%\x64;%QT%\bin;%SystemRoot%\System32
set INCLUDE=%MSVC%\include;%WINSDK%\Include\%SDKVER%\ucrt;%WINSDK%\Include\%SDKVER%\um;%WINSDK%\Include\%SDKVER%\shared;%WINSDK%\Include\%SDKVER%\winrt
set LIB=%MSVC%\lib\x64;%WINSDK%\Lib\%SDKVER%\ucrt\x64;%WINSDK%\Lib\%SDKVER%\um\x64
set LIBPATH=%MSVC%\lib\x64

cd /d C:\Users\20274\Documents\NextView

rmdir /s /q build 2>nul
mkdir build

cmake.exe -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=%QT% ^
  "-DCMAKE_CXX_COMPILER=%MSVC%\bin\Hostx64\x64\cl.exe" ^
  "-DCMAKE_MAKE_PROGRAM=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" ^
  > build\configure.log 2>&1
echo CONFIGURE_RESULT=%errorlevel% >> build\configure.log

cmake.exe --build build --config Release --parallel 4 > build\build.log 2>&1
echo BUILD_RESULT=%errorlevel% >> build\build.log
