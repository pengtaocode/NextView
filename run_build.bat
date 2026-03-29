@echo off
set MSVC=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207
set WINSDK=C:\Program Files (x86)\Windows Kits\10
set SDKVER=10.0.26100.0
set VS=C:\Program Files\Microsoft Visual Studio\2022\Community
set QT=C:\Qt\6.8.3\msvc2022_64
set CMAKE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set NINJA=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe

set PATH=%MSVC%\bin\Hostx64\x64;%WINSDK%\bin\%SDKVER%\x64;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%QT%\bin;%SystemRoot%\System32;%SystemRoot%
set INCLUDE=%MSVC%\include;%WINSDK%\Include\%SDKVER%\ucrt;%WINSDK%\Include\%SDKVER%\um;%WINSDK%\Include\%SDKVER%\shared;%WINSDK%\Include\%SDKVER%\winrt
set LIB=%MSVC%\lib\x64;%WINSDK%\Lib\%SDKVER%\ucrt\x64;%WINSDK%\Lib\%SDKVER%\um\x64
set LIBPATH=%MSVC%\lib\x64

set SRC=C:\Users\20274\Documents\NextView
set BUILD=%SRC%\build

echo === CONFIGURE === > "%SRC%\build_out.txt" 2>&1
rmdir /s /q "%BUILD%" >> "%SRC%\build_out.txt" 2>&1
mkdir "%BUILD%" >> "%SRC%\build_out.txt" 2>&1

"%CMAKE%" -S "%SRC%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_PREFIX_PATH=%QT%" ^
  "-DCMAKE_CXX_COMPILER=%MSVC%\bin\Hostx64\x64\cl.exe" ^
  "-DCMAKE_MAKE_PROGRAM=%NINJA%" >> "%SRC%\build_out.txt" 2>&1

echo CONFIGURE_EXIT=%errorlevel% >> "%SRC%\build_out.txt"

echo === BUILD === >> "%SRC%\build_out.txt" 2>&1
"%CMAKE%" --build "%BUILD%" --config Release --parallel 4 >> "%SRC%\build_out.txt" 2>&1
echo BUILD_EXIT=%errorlevel% >> "%SRC%\build_out.txt"

echo DONE >> "%SRC%\build_out.txt"
