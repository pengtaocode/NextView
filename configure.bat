@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S "C:\Users\20274\Documents\NextView" -B "C:\Users\20274\Documents\NextView\build" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64"
echo CMAKE_CONFIGURE_EXITCODE=%errorlevel%
