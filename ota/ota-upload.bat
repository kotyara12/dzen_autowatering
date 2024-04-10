@echo off

set prj_name=autowatering
set prj_dir=c:\Projects\PlatformIO\%prj_name%
set prj_web=autowatering-69ec4bc8-4b53-1613-97bb-5c7e5c27cedf
set prj_elf=%prj_dir%\ota\firmware
set prj_obj=esp32dev

echo --------------------------------------------------------------------------------------------------------------------
echo Backup ELF-file for debug...
echo --------------------------------------------------------------------------------------------------------------------

del /f /q %prj_elf%\firmware_3.elf
rename %prj_elf%\firmware_2.elf firmware_3.elf
rename %prj_elf%\firmware_1.elf firmware_2.elf
rename %prj_elf%\firmware.elf firmware_1.elf
xcopy "%prj_dir%\.pio\build\%prj_obj%\firmware.elf" %prj_elf%\ /I /H /R /K /Y 

echo --------------------------------------------------------------------------------------------------------------------
echo Upload BIN-file to FTP
echo --------------------------------------------------------------------------------------------------------------------

"C:\Program Files (x86)\WinSCP\WinSCP.com" /command "open ftp://username:password@site.com/" "cd /www/site.com/cedf5c7e5c27/%prj_web%" "put "%prj_dir%\.pio\build\%prj_obj%\firmware.bin" -transfer=binary -preservetime" "exit" /log="ota_upload.log" /ini=nul
