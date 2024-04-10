set project_dir=C:\Projects\PlatformIO\dzen_autowatering

cd %project_dir%
del /Q %project_dir%\ota\ota_upload.log
del /Q %project_dir%\ota\firmware\firmware_*.elf

git reflog expire --all --expire=now
git gc --prune=now --aggressive

pause
