@echo off
REM Scripts\test.bat — build leon_tests + ctest on Editor\build-ninja
REM Shares the clangd / configure-ninja tree (LEON_BUILD_TESTS=ON). Not build-fast.
setlocal EnableExtensions
cd /d "%~dp0.."

call "%~dp0_vsenv.bat" vsdev quiet need-ninja
if errorlevel 1 exit /b 1

if not exist "Editor\build-ninja\build.ninja" (
  cmake -S Engine -B Engine/build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release -DLEON_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  if errorlevel 1 exit /b 1
)

cmake --build Engine/build-ninja --target leon_tests
if errorlevel 1 exit /b 1

ctest --test-dir Engine/build-ninja -R "^leon\." --output-on-failure
exit /b %ERRORLEVEL%
