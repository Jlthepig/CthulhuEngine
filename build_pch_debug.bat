@echo off
call vcvars64.bat

if not exist "build\debug-windows" mkdir "build\debug-windows"

echo Building precompiled header...
cl /c /std:c++20 /Yc"pch.h" /Fp"build\debug-windows\pch.pch" /Fo"build\debug-windows\pch.obj" ^
   /I"include" ^
   /I"Libraries\GLM" ^
   /I"Libraries\flecs" ^
   /FS /MP /EHsc /Od ^
   src\pch.cpp

echo PCH built: build\debug-windows\pch.pch
