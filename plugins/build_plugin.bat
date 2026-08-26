@echo off
call "D:\Visual Studio\Main\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /O2 /MT /GS- /W3 /D_WIN32_WINNT=0x0601 /EHa /LD /Fe:"%~dp0modern_black_win64.dll" "%~dp0modern_black.cpp" /link user32.lib gdi32.lib comctl32.lib shell32.lib dwmapi.lib wininet.lib /OPT:REF /OPT:ICF
del "%~dp0modern_black.obj" "%~dp0modern_black_win64.exp" "%~dp0modern_black_win64.lib" 2>nul
echo Done!
