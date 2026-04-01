@echo off
cd /d "%~dp0"
echo 正在生成图标...

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$d='%~dp0'.TrimEnd('\'); $tmp=[IO.Path]::GetTempFileName()+'.py';" ^
    "[IO.File]::WriteAllText($tmp, @'" ^
    "import os,sys,struct,io`n" ^
    "from PIL import Image,ImageDraw`n" ^
    "DIR=r''+chr(36)+'d'`n" ^
    "'@ -replace 'chr\(36\)\+.d.', repr($d).strip(chr(39)));" ^
    "Write-Host $tmp; python $tmp; Remove-Item $tmp"

if %errorlevel% neq 0 (
    echo 错误：请确保已安装 Python 和 Pillow
    echo 安装：python -m pip install pillow
    pause
    exit /b 1
)
pause
