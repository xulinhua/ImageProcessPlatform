@echo off
setlocal EnableExtensions
set "INTDIR=%~1"
set "MOC=%~2"
set "SLN=%~dp0..\"
if "%INTDIR%"=="" exit /b 1
if not exist "%MOC%" (
  echo moc.exe not found: %MOC%
  exit /b 1
)
set "D=-DHSIMAGE_STATIC -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -DUNICODE -D_UNICODE -DWIN32"
set "I=-I"%SLN%include" -I"%SLN%VisionCommon" -I"%SLN%VisionCommon\common" -I"%SLN%VisionCore" -I"%SLN%VisionCore\model" -I"%SLN%VisionCore\tool" -I"%SLN%VisionCore\job" -I"%SLN%VisionCore\station" -I"%SLN%VisionCore\solution" -I"%SLN%VisionCore\common" -I"%SLN%VisionAuth" -I"%SLN%VisionAuth\session" -I"%SLN%VisionAuth\user" -I"%SLN%VisionLang" -I"%SLN%VisionLang\language" -I"%SLN%VisionLog" -I"%SLN%VisionLog\log" -I"%SLN%VisionCfgMgr" -I"%SLN%VisionCfgMgr\\json" -I"%SLN%VisionCfgMgr\\store" -I"%SLN%VisionWidgets" -I"%SLN%VisionWidgets\theme" -I"%SLN%VisionUi" -I"%SLN%VisionUi\main" -I"%SLN%VisionToolUi" -I"%SLN%VisionToolUi\editor" -I"%SLN%VisionToolUi\debug" -I"%SLN%VisionToolUi\register" -I"%SLN%VisionToolUi\draw" -I"%SLN%third_party\HsImageCtrl\include" -I"%SLN%..\HsImage\HsCommData\include""
call "%~dp0run_moc.bat" "%SLN%VisionToolUi\editor\ToolEditorBase.h" "%INTDIR%moc_ToolEditorBase.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionToolUi\editor\ToolIoPanel.h" "%INTDIR%moc_ToolIoPanel.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionToolUi\debug\ToolDebugHost.h" "%INTDIR%moc_ToolDebugHost.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
exit /b %ERRORLEVEL%
