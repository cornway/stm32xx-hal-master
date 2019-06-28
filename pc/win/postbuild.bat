set armpath=%1
set objpath=%2
set objname=%3

if "%armpath%" == "" exit 1
if "%objpath%" == "" set objpath=obj
if "%objname%" == "" set objname=bin

set armccpath=%armpath%\ARMCC

set axfpath=%objpath%\Project.axf
set outpath=%objpath%\text\%objname%.txt
set outbin=%objpath%\bin\%objname%.bin
set fromelfpath=%armccpath%\bin\fromelf.exe

%fromelfpath% --text -s -t --output=%outpath% "%axfpath%"
%fromelfpath% --bin --output=%outbin% "%axfpath%"
exit 0