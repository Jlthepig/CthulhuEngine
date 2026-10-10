@echo off
setlocal

:: --- Always work from the directory containing this .bat file ---
cd /d "%~dp0"

echo [INFO] Working directory: %CD%

:: --- Locate and call vcvars64.bat ---
:: Adjust this path if your VS install location/edition differs
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" (
echo [ERROR] vcvars64.bat not found at:
echo %VCVARS%
echo Edit this script and set the correct path for your VS install.
exit /b 1
)

call "%VCVARS%"
if errorlevel 1 (
echo [ERROR] Failed to initialize MSVC environment.
exit /b 1
)

echo.
echo [INFO] MSVC environment initialized.
echo [INFO] LIB=%LIB%
echo.

:: --- Verify the correct project.kmake exists ---
if not exist "project.kmake" (
echo [ERROR] project.kmake not found in:
echo %CD%
exit /b 1
)

echo [INFO] Using project:
echo %CD%\project.kmake
echo.

:: --- Run the actual build ---
kalamake --compile project.kmake release-windows

if errorlevel 1 (
echo [ERROR] kalamake build failed.
exit /b 1
)

echo.
echo [INFO] Build completed successfully.
exit /b 0
