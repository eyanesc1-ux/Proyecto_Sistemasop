@echo off
REM ejecutar.bat - Ejecuta el simulador en Windows / Windows Server Core
REM Uso:  ejecutar.bat            (todo con valores por defecto)
REM       ejecutar.bat -n 500     (cualquier opcion de simplanif.exe)
cd /d "%~dp0"
bin\windows\simplanif.exe %*
echo.
echo Resultados en: %~dp0salida
echo   consola.txt, comparativa.csv, barrido_quantum.csv, reporte.html,
echo   bitacoras\*.log y graficas\*.svg
