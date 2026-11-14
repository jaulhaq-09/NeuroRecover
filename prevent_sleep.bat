@echo off
title NeuroRecover - Keeping System Awake
echo.
echo  ============================================
echo    NeuroRecover Sleep Prevention
echo  ============================================
echo.
echo  This window keeps your laptop awake during
echo  the recovery scan. 
echo.
echo  DO NOT CLOSE THIS WINDOW during scanning!
echo  Close it only after scan completes.
echo.

:: Set power plan to High Performance (no sleep)
powercfg /change standby-timeout-ac 0
powercfg /change standby-timeout-dc 0
powercfg /change monitor-timeout-ac 0
powercfg /change monitor-timeout-dc 0
powercfg /change hibernate-timeout-ac 0
powercfg /change hibernate-timeout-dc 0

echo  ✅ Sleep disabled for this session
echo.
echo  Keeping system awake... (press Ctrl+C when scan is done)
echo.

:: Keep sending keystrokes to prevent sleep every 60 seconds
:loop
timeout /t 60 /nobreak > nul
powercfg /requests > nul 2>&1
echo  [%time%] System kept awake...
goto loop