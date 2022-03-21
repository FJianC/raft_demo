@echo off

set cur_dir=%~dp0
set build_dir=build

REM echo %cur_dir%%build_dir%
REM if exist %cur_dir%%build_dir% (
    REM rd /s /q %cur_dir%%build_dir%
REM )

mkdir %cur_dir%%build_dir%

cd %cur_dir%%build_dir%

cmake ../ -G "Visual Studio 17 2022" -A x64
cd ..
pause
