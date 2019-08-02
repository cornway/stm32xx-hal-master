set objpath=%1
set objname=%2

if "%objpath%" == "" set objpath=obj
if "%objname%" == "" set objname=bin

set sendfile=%objpath%/bin/%objname%.bin

mode COM1 BAUD=115200 PARITY=n DATA=8

echo "bsp stdin > %objname%.bin -x -a +w -t 1000 -f XXXXXXXX" > COM1
copy %sendfile% > COM1
echo XXXXXXXX > COM1

exit 0