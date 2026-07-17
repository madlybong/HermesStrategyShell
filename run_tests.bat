@echo off
setlocal

:: Navigate to the build directory
if not exist "build\Release\tests.exe" (
    echo [ERROR] Test executable not found. Please build the 'tests' target first.
    echo Try running: build.sh
    exit /b 1
)

echo [INFO] Running HermesFutCashStrategy tests...
build\Release\tests.exe %*

if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] All tests passed!
) else (
    echo [ERROR] Some tests failed.
)

endlocal
exit /b %ERRORLEVEL%
