@echo off
call vcvars64.bat

if not exist "build\release-windows" mkdir "build\release-windows"

echo Building precompiled header...
cl /c /std:c++20 /MD /Yc"pch.h" /Fp"build\release-windows\pch.pch" /Fo"build\release-windows\pch.obj" ^
   /I"include" ^
   /I"Libraries\GLM" ^
   /I"Libraries\flecs" ^
   /I"Libraries\simdjson" ^
   /I"Libraries\glad" ^
   /I"Libraries\GLFW\include\GLFW" ^
   /I"Libraries\Stb_image" ^
   /I"Libraries\ImGui" ^
   /I"Libraries\JoltPhysics-5.5.0" ^
   /I"Libraries\FASTTGLTF" ^
   /FS /MP /EHsc ^
   src\pch.cpp

if %ERRORLEVEL% NEQ 0 (
    echo BUILD FAILED
    pause
) else (
    echo PCH built: build\release-windows\pch.pch
)

pause
