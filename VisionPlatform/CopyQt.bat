@echo off
setlocal EnableExtensions
set CONF=%~1
set DST=%~2
if "%DST:~-1%"=="." set DST=%DST:~0,-1%
if "%DST:~-1%"=="\" set DST=%DST:~0,-1%
set QT=%~3
if "%DST%"=="" exit /b 0
if not exist "%DST%" mkdir "%DST%"

set S=
if /I "%CONF%"=="Debug" set S=d
if not "%QT%"=="" (
  call :copydll "%QT%\bin\Qt5Core%S%.dll" "%DST%"
  call :copydll "%QT%\bin\Qt5Gui%S%.dll" "%DST%"
  call :copydll "%QT%\bin\Qt5Widgets%S%.dll" "%DST%"
  call :copydll "%QT%\bin\Qt5Network%S%.dll" "%DST%"
  call :copydll "%QT%\bin\Qt5SerialPort%S%.dll" "%DST%"
  call :copydll "%QT%\bin\Qt5Svg%S%.dll" "%DST%"
  call :copydll "%QT%\bin\libGLESv2%S%.dll" "%DST%"
  call :copydll "%QT%\bin\libEGL%S%.dll" "%DST%"
  if not exist "%DST%\platforms" mkdir "%DST%\platforms"
  call :copydll "%QT%\plugins\platforms\qwindows%S%.dll" "%DST%\platforms"
  if not exist "%DST%\imageformats" mkdir "%DST%\imageformats"
  call :copydll "%QT%\plugins\imageformats\qsvg%S%.dll" "%DST%\imageformats"
)

set SAMPLES=%~4
if not "%SAMPLES%"=="" (
  if not exist "%DST%\samples" mkdir "%DST%\samples"
  if exist "%SAMPLES%\*.bmp" copy /Y "%SAMPLES%\*.bmp" "%DST%\samples" >nul 2>nul
)

rem tool tree icons -> exe\icons\tools
set ICONSRC=%~dp0icons\tools
if exist "%ICONSRC%\*.svg" (
  if not exist "%DST%\icons\tools" mkdir "%DST%\icons\tools"
  copy /Y "%ICONSRC%\*.svg" "%DST%\icons\tools" >nul 2>nul
)
if exist "%ICONSRC%\*.png" (
  if not exist "%DST%\icons\tools" mkdir "%DST%\icons\tools"
  copy /Y "%ICONSRC%\*.png" "%DST%\icons\tools" >nul 2>nul
)

rem main UI icons -> exe\icons\ui
set UIICON=%~dp0icons\ui
if exist "%UIICON%\*.svg" (
  if not exist "%DST%\icons\ui" mkdir "%DST%\icons\ui"
  copy /Y "%UIICON%\*.svg" "%DST%\icons\ui" >nul 2>nul
)

rem app icon -> exe\icons\app
set APPICON=%~dp0icons\app
if exist "%APPICON%\app.png" (
  if not exist "%DST%\icons\app" mkdir "%DST%\icons\app"
  copy /Y "%APPICON%\app.png" "%DST%\icons\app" >nul 2>nul
  if exist "%APPICON%\app.ico" copy /Y "%APPICON%\app.ico" "%DST%\icons\app" >nul 2>nul
)

set HALROOT=%~5
set HALARCH=%~6
if "%HALARCH%"=="" set HALARCH=x64-win64
if not "%HALROOT%"=="" (
  call :copydll "%HALROOT%\bin\%HALARCH%\halcon.dll" "%DST%"
  call :copydll "%HALROOT%\bin\%HALARCH%\halconcpp.dll" "%DST%"
  call :copydll "%HALROOT%\bin\%HALARCH%\hAcqFile.dll" "%DST%"
  call :copydll "%HALROOT%\bin\%HALARCH%\hAcqDirectFile.dll" "%DST%"
)

rem PCL / Open3D runtime next to exe (from third_party)
set ROOT=%~dp0
set PCLBIN=%ROOT%third_party\PCL\bin
set O3DBIN=%ROOT%third_party\Open3D\bin
set PCLS=
if /I "%CONF%"=="Debug" set PCLS=d
for %%M in (common filters sample_consensus segmentation registration search kdtree octree features ml) do (
  call :copydll "%PCLBIN%\pcl_%%M%PCLS%.dll" "%DST%"
)
call :copydll "%O3DBIN%\Open3D.dll" "%DST%"
call :copydll "%O3DBIN%\tbb12.dll" "%DST%"

:done
endlocal
exit /b 0

:copydll
if "%~1"=="" exit /b 0
if not exist "%~1" exit /b 0
copy /Y "%~1" "%~2\" >nul 2>nul
if errorlevel 1 echo [CopyQt] skip (in use or failed): %~nx1
exit /b 0
