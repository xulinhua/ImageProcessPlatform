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
set "I=-I"%SLN%include" -I"%SLN%VisionCommon" -I"%SLN%VisionCommon\common" -I"%SLN%VisionCore" -I"%SLN%VisionCore\model" -I"%SLN%VisionCore\tool" -I"%SLN%VisionCore\job" -I"%SLN%VisionCore\station" -I"%SLN%VisionCore\solution" -I"%SLN%VisionCore\common" -I"%SLN%VisionCore\comm" -I"%SLN%VisionCore\device" -I"%SLN%VisionCore\var" -I"%SLN%VisionCore\ui" -I"%SLN%VisionComm" -I"%SLN%VisionAuth" -I"%SLN%VisionAuth\session" -I"%SLN%VisionAuth\user" -I"%SLN%VisionAuth\audit" -I"%SLN%VisionLang" -I"%SLN%VisionLang\language" -I"%SLN%VisionLog" -I"%SLN%VisionLog\log" -I"%SLN%VisionCfgMgr" -I"%SLN%VisionCfgMgr\\json" -I"%SLN%VisionCfgMgr\\store" -I"%SLN%VisionWidgets" -I"%SLN%VisionWidgets\theme" -I"%SLN%VisionUi" -I"%SLN%VisionUi\main" -I"%SLN%VisionUi\workspace" -I"%SLN%VisionUi\dialog" -I"%SLN%VisionToolUi" -I"%SLN%VisionToolUi\editor" -I"%SLN%VisionToolUi\debug" -I"%SLN%VisionToolUi\register""
call "%~dp0run_moc.bat" "%SLN%VisionUi\main\MainWindow.h" "%INTDIR%moc_MainWindow.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\workspace\DesignWorkspace.h" "%INTDIR%moc_DesignWorkspace.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\workspace\RuntimeWorkspace.h" "%INTDIR%moc_RuntimeWorkspace.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\workspace\StationRuntimePanel.h" "%INTDIR%moc_StationRuntimePanel.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\workspace\RecheckWorkspace.h" "%INTDIR%moc_RecheckWorkspace.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\workspace\JobCanvas.h" "%INTDIR%moc_JobCanvas.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\workspace\JobFlowCanvasPanel.h" "%INTDIR%moc_JobFlowCanvasPanel.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\StationMgrDlg.h" "%INTDIR%moc_StationMgrDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\StationDlg.h" "%INTDIR%moc_StationDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\DeviceMgrDlg.h" "%INTDIR%moc_DeviceMgrDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\DeviceDlg.h" "%INTDIR%moc_DeviceDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\CommMgrDlg.h" "%INTDIR%moc_CommMgrDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\JobDlg.h" "%INTDIR%moc_JobDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\JobRstRulePanel.h" "%INTDIR%moc_JobRstRulePanel.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\JobRstRuleDlg.h" "%INTDIR%moc_JobRstRuleDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\LoginDlg.h" "%INTDIR%moc_LoginDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\UserMgrDlg.h" "%INTDIR%moc_UserMgrDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\ChangePwdDlg.h" "%INTDIR%moc_ChangePwdDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\UserEditDlg.h" "%INTDIR%moc_UserEditDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\PermDlg.h" "%INTDIR%moc_PermDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\LogDlg.h" "%INTDIR%moc_LogDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\SettingsDlg.h" "%INTDIR%moc_SettingsDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\RuntimeViewDlg.h" "%INTDIR%moc_RuntimeViewDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\GlobalVarDlg.h" "%INTDIR%moc_GlobalVarDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\UiDesignerDlg.h" "%INTDIR%moc_UiDesignerDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\workspace\UiPageHost.h" "%INTDIR%moc_UiPageHost.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\AboutDlg.h" "%INTDIR%moc_AboutDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\VersionDlg.h" "%INTDIR%moc_VersionDlg.cpp" "%MOC%" %I% %D%
if errorlevel 1 exit /b 1
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\CalibMgrDlg.h" "%INTDIR%moc_CalibMgrDlg.cpp" "%MOC%" %I% %D%
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\ToolLibDlg.h" "%INTDIR%moc_ToolLibDlg.cpp" "%MOC%" %I% %D%
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\ParaAdaptDlg.h" "%INTDIR%moc_ParaAdaptDlg.cpp" "%MOC%" %I% %D%
call "%~dp0run_moc.bat" "%SLN%VisionUi\dialog\CompositeMgrDlg.h" "%INTDIR%moc_CompositeMgrDlg.cpp" "%MOC%" %I% %D%
exit /b %ERRORLEVEL%
