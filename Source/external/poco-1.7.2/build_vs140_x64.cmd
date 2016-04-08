@echo off
if defined VCINSTALLDIR (
call "%VCINSTALLDIR%\vcvarsall.bat x64")
buildwin 140 build shared both x64
