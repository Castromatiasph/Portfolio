@echo off
setlocal
rem Compila el proyecto y guarda los errores en "errores_de_compilacion.txt" (se abre solo al terminar).
rem Cerra Unreal Engine antes de correrlo.
set "UE=C:\Program Files\Epic Games\UE_5.8"
if not exist "%UE%\Engine\Build\BatchFiles\Build.bat" (
  echo No encuentro Unreal Engine en: %UE%
  set /p UE=Escribi la carpeta donde instalaste Unreal 5.8 ^(sin barra final^): 
)
set "PROJ=%~dp0..\UnrealPenalPatines.uproject"
set "LOG=%~dp0..\compilar_log.txt"
set "ERR=%~dp0..\errores_de_compilacion.txt"
echo Compilando... puede tardar varios minutos. No cierres esta ventana.
call "%UE%\Engine\Build\BatchFiles\Build.bat" UnrealPenalPatinesEditor Win64 Development -Project="%PROJ%" -WaitMutex > "%LOG%" 2>&1
echo. > "%ERR%"
findstr /i /c:"error" /c:"Result:" "%LOG%" >> "%ERR%"
echo.
echo Listo. Se va a abrir el archivo con los errores. Copiá su contenido y pegalo en el chat.
notepad "%ERR%"
pause
