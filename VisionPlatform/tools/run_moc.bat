@echo off
setlocal EnableExtensions
set "HEADER=%~1"
set "OUTFILE=%~2"
set "MOC=%~3"
if not exist "%MOC%" (
  echo moc.exe not found: %MOC%
  exit /b 1
)
for %%I in ("%OUTFILE%") do set "OUTDIR=%%~dpI"
if "%OUTDIR:~-1%"=="\" set "OUTDIR=%OUTDIR:~0,-1%"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
shift
shift
shift
set "ARGS="
:argloop
if "%~1"=="" goto run
set "ARGS=%ARGS% %1"
shift
goto argloop
:run
"%MOC%" "%HEADER%" -o "%OUTFILE%" %ARGS%
exit /b %ERRORLEVEL%
