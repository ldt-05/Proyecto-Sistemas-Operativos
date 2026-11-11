@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

if exist "C:\msys64\ucrt64\bin\gcc.exe" (
    set "PATH=C:\msys64\ucrt64\bin;%PATH%"
)

set "FORZAR_COMPILACION=0"
set "SOLO_COMPILAR=0"
if /i "%~1"=="--compilar" set "FORZAR_COMPILACION=1"
if /i "%~1"=="-c" set "FORZAR_COMPILACION=1"
if /i "%~1"=="--solo-compilar" (
    set "FORZAR_COMPILACION=1"
    set "SOLO_COMPILAR=1"
)

set "NECESITA_COMPILAR=%FORZAR_COMPILACION%"
if not exist "simulador_so.exe" set "NECESITA_COMPILAR=1"
if not exist "gui\simulador_gui.exe" set "NECESITA_COMPILAR=1"

if "%NECESITA_COMPILAR%"=="0" (
    powershell -NoProfile -Command "$exe=(Get-Item 'gui\simulador_gui.exe').LastWriteTime; $cli=(Get-Item 'simulador_so.exe').LastWriteTime; $ref=if($cli -lt $exe){$cli}else{$exe}; $nuevos=Get-ChildItem 'src\*.c','include\*.h','gui\*.c','gui\*.h' | Where-Object { $_.LastWriteTime -gt $ref }; if ($nuevos) { exit 1 } else { exit 0 }" >nul 2>nul
    if errorlevel 1 set "NECESITA_COMPILAR=1"
)

if "%NECESITA_COMPILAR%"=="1" (
    where gcc >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] No se encontro gcc en el PATH ni en C:\msys64\ucrt64\bin.
        echo         Instala MSYS2 UCRT64 ^(gcc y raylib^) e intenta de nuevo.
        if "%SOLO_COMPILAR%"=="0" pause
        exit /b 1
    )

    echo [1/2] Compilando version consola ^(simulador_so.exe^)...
    gcc src/main.c src/cli.c src/scheduler.c -o simulador_so.exe -std=c11 -O2 -Wall -Wextra -pthread -lmingw32 -mconsole
    if errorlevel 1 (
        echo [ERROR] Fallo la compilacion de simulador_so.exe.
        if "%SOLO_COMPILAR%"=="0" pause
        exit /b 1
    )

    echo [2/2] Compilando interfaz grafica Raylib ^(gui\simulador_gui.exe^)...
    if exist "C:\msys64\ucrt64\include\raylib.h" (
        gcc gui/gui_main.c gui/gui.c gui/gui_style.c gui/gui_data.c gui/gui_gantt.c src/cli.c src/scheduler.c -o gui/simulador_gui.exe -std=c11 -O2 -Wall -Wextra -pthread -IC:/msys64/ucrt64/include -LC:/msys64/ucrt64/lib -lraylib -lm
    ) else (
        gcc gui/gui_main.c gui/gui.c gui/gui_style.c gui/gui_data.c gui/gui_gantt.c src/cli.c src/scheduler.c -o gui/simulador_gui.exe -std=c11 -O2 -Wall -Wextra -pthread -lraylib -lm
    )
    if errorlevel 1 (
        echo [ERROR] Fallo la compilacion de gui\simulador_gui.exe.
        echo         - Si ya tenias una ventana abierta, cierrala primero con 'exit'.
        echo         - Si falta raylib: pacman -S mingw-w64-ucrt-x86_64-raylib
        if "%SOLO_COMPILAR%"=="0" pause
        exit /b 1
    )
    echo [OK] Compilacion completada.
)

if "%SOLO_COMPILAR%"=="1" exit /b 0

echo [INFO] Iniciando Simulador SO ^(CLI + Interfaz Grafica Raylib^)...
"gui\simulador_gui.exe"
set "SALIDA=!ERRORLEVEL!"
if not "!SALIDA!"=="0" (
    echo.
    echo [ERROR] El programa finalizo con codigo de error !SALIDA!.
    pause
    exit /b !SALIDA!
)
