@echo off
chcp 65001 > /dev/null

set "VS=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "MSVC=%VS%\VC\Tools\MSVC\14.44.35207"
set "WINSDK=C:\Program Files (x86)\Windows Kits\10"
set "SDKVER=10.0.26100.0"
set "CMAKE_EXE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA_EXE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "QT_DIR=C:\Qt\6.8.3\msvc2022_64"
set "SRC_DIR=C:\Users\20274\Documents\NextView"
set "BUILD_DIR=C:\Users\20274\Documents\NextView\build"

set "PATH=%MSVC%\bin\Hostx64\x64;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%WINSDK%\bin\%SDKVER%\x64;%QT_DIR%\bin;%SystemRoot%\System32;%SystemRoot%"
set "INCLUDE=%MSVC%\include;%WINSDK%\Include\%SDKVER%\ucrt;%WINSDK%\Include\%SDKVER%\um;%WINSDK%\Include\%SDKVER%\shared;%WINSDK%\Include\%SDKVER%\winrt"
set "LIB=%MSVC%\lib\x64;%WINSDK%\Lib\%SDKVER%\ucrt\x64;%WINSDK%\Lib\%SDKVER%\um\x64"
set "LIBPATH=%MSVC%\lib\x64"

echo Starting configure...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

"%CMAKE_EXE%" -S "%SRC_DIR%" -B "%BUILD_DIR%" -G "Ninja" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=%QT_DIR%" "-DCMAKE_CXX_COMPILER=%MSVC%\bin\Hostx64\x64\cl.exe" "-DCMAKE_MAKE_PROGRAM=%NINJA_EXE%"
if %errorlevel% neq 0 (
    echo CONFIGURE_FAILED
    exit /b 1
)
echo CONFIGURE_OK

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --parallel 4
if %errorlevel% neq 0 (
    echo BUILD_FAILED
    exit /b 1
)
echo BUILD_OK

"%QT_DIR%\bin\windeployqt.exe" --release --multimedia "%BUILD_DIR%\NextView.exe"
echo DEPLOY_DONE
