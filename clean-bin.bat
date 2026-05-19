REM --------------------------------------
REM clean-bin.bat
REM --------------------------------------
DEL /S /Q /F *.ilk
DEL /S /Q /F *.bsc
REM ------------------
REM bin
REM ------------------
RMDIR /S /Q .\bin\msvc\Debug\obj
RMDIR /S /Q .\bin\msvc\Release\obj
REM ------------------
REM build
REM ------------------
REM msvc2022
REM ----------
RMDIR /S /Q .\build\msvc2022\_UpgradeReport_Files
RMDIR /S /Q .\build\msvc2022\.vs
RMDIR /S /Q .\build\msvc2022\ipch
RMDIR /S /Q .\build\msvc2022\Debug
RMDIR /S /Q .\build\msvc2022\Release
REM --------------------------------------
