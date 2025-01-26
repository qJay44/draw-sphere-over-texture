@echo off
cls

Rem Build
cmake -S . -B Build -G "Visual Studio 17 2022"
cmake --build Build --config Release

Rem Copy dlls
cd Build\\Release
if not exist Run mkdir Run
cd Run
move /y ..\\MyProject.exe .

Rem Lauch
MyProject.exe
cd ..\..\..

