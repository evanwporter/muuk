@echo off
setlocal

set "SRC=build\release\bin\bin.exe"
set "DEST=build\release\bin\muuk.exe"

if exist "%DEST%" (
    echo Removing existing %DEST%...
    del /f /q "%DEST%"
)

if exist "%SRC%" (
    echo Renaming %SRC% to %DEST%...
    ren "%SRC%" "muuk.exe"
    echo Rename successful!
) else (
    echo Error: %SRC% not found!
)

endlocal
