@echo off
setlocal
set EXE=%~1
if "%EXE%"=="" exit /b 0
set BASE=%~dpn1
if exist "%BASE%.pdb.old" del /F /Q "%BASE%.pdb.old" 2>nul
if exist "%BASE%.pdb" move /Y "%BASE%.pdb" "%BASE%.pdb.old" >nul 2>nul
if exist "%BASE%.ilk.old" del /F /Q "%BASE%.ilk.old" 2>nul
if exist "%BASE%.ilk" move /Y "%BASE%.ilk" "%BASE%.ilk.old" >nul 2>nul
if exist "%EXE%.old" del /F /Q "%EXE%.old" 2>nul
if exist "%EXE%" move /Y "%EXE%" "%EXE%.old" >nul 2>nul
exit /b 0
