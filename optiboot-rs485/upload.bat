REM Example: upload comport# address
@echo off
set arduino_build_folder_location=C:\Users\MATTSM~1\AppData\Local\Temp
set comport=%1
set address=%2
set programming_baudrate=19200
set regular_baudrate=57600 REM What speed do the Unos usually talk when in regular christmas playing mode

REM Find arduino build folder
for /f %%i in ('ls %arduino_build_folder_location% ^| grep arduino_build') do SET "sub_folder_name=%%i"

REM Find hex file inside...
for /f %%i in ('ls %arduino_build_folder_location%\%sub_folder_name% ^| grep .hex') do SET "filename=%%i"

echo Found: %arduino_build_folder_location%\%sub_folder_name%\%filename%

REM Set serial port to running program baud rate settings
mode COM%comport% BAUD=%regular_baudrate% PARITY=n DATA=8

REM Send serial command to go into bootloader
REM I Have to send a 0 then the digit for the address so two characters
@echo | set /p="0" > in.text
@echo | set /p=%address% >> in.text
@echo | set /p="00 05 00 00 00" >> in.text
echo sending command:
type in.text
@echo.
sfk filter in.text +hextobin out.bin
copy out.bin \\.\COM%comport% /B
del out.bin
del in.text

REM Pause
echo Pausing to let arduino catch up
REM ping 127.0.0.1 -n 3 > nul

echo avrdude -p m328p -b %programming_baudrate% -c arduino -P\\.\COM%comport% -v -U flash:w:"%arduino_build_folder_location%\%sub_folder_name%\%filename%":i
avrdude -p m328p -b %programming_baudrate% -c arduino -P\\.\COM%comport% -v -U flash:w:"%arduino_build_folder_location%\%sub_folder_name%\%filename%":i
