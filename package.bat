@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul

echo ================================================
echo  NextView ^— Windows 完整打包脚本
echo ================================================
echo.

:: ── 路径配置 ──────────────────────────────────────
set QT_DIR=C:\Qt\6.8.3\msvc2022_64
set VS_CMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat
set CMAKE_EXE=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build
set DIST_DIR=%PROJECT_DIR%dist
set OUT_EXE=%USERPROFILE%\Desktop\NextView.exe
set EVB_EXE=C:\Program Files (x86)\Enigma Virtual Box\enigmavbconsole.exe
set EVB_FILE=%PROJECT_DIR%nextview_pack.evb

:: ── Step 1: 编译 Release ──────────────────────────
echo [1/5] 编译 Release...
call "%VS_CMD%" -arch=x64 -host_arch=x64 >nul 2>&1
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"
"%CMAKE_EXE%" .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_CXX_COMPILER=cl >nul 2>&1
"%CMAKE_EXE%" --build . --config Release --parallel
if %errorlevel% neq 0 (
    echo [错误] 编译失败！
    popd & pause & exit /b 1
)
popd
echo       完成：%BUILD_DIR%\NextView.exe
echo.

:: ── Step 2: 整理 dist 目录 ────────────────────────
echo [2/5] 整理发布目录...
if exist "%DIST_DIR%" rd /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"

copy /y "%BUILD_DIR%\NextView.exe" "%DIST_DIR%\" >nul

for %%f in ("%BUILD_DIR%\*.dll") do copy /y "%%f" "%DIST_DIR%\" >nul

for %%d in (platforms imageformats multimedia iconengines styles networkinformation tls generic translations) do (
    if exist "%BUILD_DIR%\%%d" (
        xcopy /e /i /q /y "%BUILD_DIR%\%%d" "%DIST_DIR%\%%d\" >nul
    )
)
echo       完成：%DIST_DIR%
echo.

:: ── Step 3: 查找 ffmpeg ───────────────────────────
echo [3/5] 检查 FFmpeg...
set FFMPEG_FOUND=0

for %%p in (
    "C:\ffmpeg\bin\ffmpeg.exe"
    "C:\Program Files\ffmpeg\bin\ffmpeg.exe"
    "%USERPROFILE%\ffmpeg\bin\ffmpeg.exe"
) do (
    if exist %%p (
        for %%q in (%%p) do (
            set "FFDIR=%%~dpq"
        )
        copy /y "!FFDIR!ffmpeg.exe"  "%DIST_DIR%\ffmpeg.exe"  >nul 2>&1
        copy /y "!FFDIR!ffprobe.exe" "%DIST_DIR%\ffprobe.exe" >nul 2>&1
        set FFMPEG_FOUND=1
    )
)

for /d %%d in ("%USERPROFILE%\Downloads\ffmpeg*") do (
    if exist "%%d\bin\ffmpeg.exe" (
        copy /y "%%d\bin\ffmpeg.exe"  "%DIST_DIR%\ffmpeg.exe"  >nul
        copy /y "%%d\bin\ffprobe.exe" "%DIST_DIR%\ffprobe.exe" >nul
        set FFMPEG_FOUND=1
    )
)

if !FFMPEG_FOUND!==1 (
    echo       已找到并复制 ffmpeg.exe / ffprobe.exe
) else (
    echo.
    echo   [警告] 未找到 ffmpeg.exe，视频预览生成功能将不可用！
    echo.
    echo   请前往下载 FFmpeg Windows Essential 版：
    echo   https://github.com/BtbN/FFmpeg-Builds/releases
    echo   （下载 ffmpeg-master-latest-win64-gpl.zip）
    echo.
    echo   解压后将 ffmpeg.exe 和 ffprobe.exe 复制到：
    echo   %DIST_DIR%\
    echo   然后继续执行后续步骤。
    echo.
    pause
)
echo.

:: ── Step 4: 生成 EVB 项目文件（Python） ──────────
echo [4/5] 生成 Enigma Virtual Box 项目文件...
python3 -c "
import os, sys

dist_dir = sys.argv[1].replace('\\\\', '/')
out_exe  = sys.argv[2].replace('\\\\', '/')
evb_file = sys.argv[3].replace('\\\\', '/')

lines = []
lines.append('<?xml version=\"1.0\" encoding=\"utf-8\"?>')
lines.append('<EnigmaVirtualBox>')
lines.append('  <InputFile>' + dist_dir + '/NextView.exe</InputFile>')
lines.append('  <OutputFile>' + out_exe + '</OutputFile>')
lines.append('  <Files>')
lines.append('    <Enabled>true</Enabled>')
lines.append('    <DeleteExtractedOnExit>true</DeleteExtractedOnExit>')
lines.append('    <CompressFiles>true</CompressFiles>')

def file_entry(path, name):
    return [
        '    <File>',
        '      <Type>3</Type>',
        '      <Name>' + name + '</Name>',
        '      <File>' + path + '</File>',
        '      <ActiveX>false</ActiveX>',
        '      <ActiveXInstall>false</ActiveXInstall>',
        '    </File>'
    ]

for f in os.listdir(dist_dir):
    fp = os.path.join(dist_dir, f)
    if os.path.isfile(fp) and f != 'NextView.exe':
        lines.extend(file_entry(fp.replace(chr(92), '/'), f))

lines.append('  </Files>')
lines.append('  <Folders>')
lines.append('    <Enabled>true</Enabled>')

subdirs = [d for d in os.listdir(dist_dir) if os.path.isdir(os.path.join(dist_dir, d))]
for sd in subdirs:
    sd_path = os.path.join(dist_dir, sd)
    lines.append('    <Folder>')
    lines.append('      <Type>3</Type>')
    lines.append('      <Name>' + sd + '</Name>')
    lines.append('      <Files>')
    for f in os.listdir(sd_path):
        fp = os.path.join(sd_path, f)
        if os.path.isfile(fp):
            lines.extend(['  ' + l for l in file_entry(fp.replace(chr(92), '/'), f)])
    lines.append('      </Files>')
    lines.append('    </Folder>')

lines.append('  </Folders>')
lines.append('  <VirtualRegistry><Enabled>false</Enabled></VirtualRegistry>')
lines.append('  <Options>')
lines.append('    <ShareVirtualSystem>false</ShareVirtualSystem>')
lines.append('    <MapExecutableWithTemporaryFile>false</MapExecutableWithTemporaryFile>')
lines.append('    <AllowRunningOfVirtualExeFiles>true</AllowRunningOfVirtualExeFiles>')
lines.append('  </Options>')
lines.append('</EnigmaVirtualBox>')

with open(evb_file, 'w', encoding='utf-8') as fout:
    fout.write('\n'.join(lines))
print('OK')
" "%DIST_DIR%" "%OUT_EXE%" "%EVB_FILE%"

if %errorlevel% neq 0 (
    echo [错误] EVB 项目文件生成失败
    pause & exit /b 1
)
echo       已生成：%EVB_FILE%
echo.

:: ── Step 5: 打包 ─────────────────────────────────
echo [5/5] 打包为单 EXE...
if exist "%EVB_EXE%" (
    echo       调用 Enigma Virtual Box Console...
    "%EVB_EXE%" "%EVB_FILE%"
    if !errorlevel!==0 (
        echo.
        echo ================================================
        echo  打包成功！桌面文件：
        echo  %OUT_EXE%
        echo ================================================
    ) else (
        echo [错误] 打包失败，请手动用 GUI 打开 EVB 文件处理：
        echo       %EVB_FILE%
    )
) else (
    echo.
    echo   未检测到 Enigma Virtual Box，请按以下步骤：
    echo.
    echo   1. 下载安装（免费，约 3 MB）：
    echo      https://enigmaprotector.com/en/downloads/enigmavirtualbox
    echo.
    echo   2. 安装后 打开 Enigma Virtual Box GUI
    echo.
    echo   3. 菜单 File → Open，选择：
    echo      %EVB_FILE%
    echo.
    echo   4. 点击右下角 [Process] 按钮
    echo.
    echo   5. 单 EXE 输出到桌面：NextView.exe
    echo.
    echo   ─── 或者直接使用便携版 ────────────────────
    echo   dist\ 目录本身即为完整便携包，
    echo   可压缩成 zip 发给用户，解压后双击 NextView.exe 运行。
)
echo.
pause
