echo "%1"

set armpath=%1
set armccpath=%armpath%\ARMCC

if "%armpath%" == "" exit 1

set objpath=obj
set axfpath=%objpath%\Project.axf
set outpath=%objpath%\text\symtab.txt
set outbin=%objpath%\bin\exe.bin
set fromelfpath=%armccpath%\bin\fromelf.exe

%fromelfpath% --text -s -t --output=%outpath% "%axfpath%"
%fromelfpath% --bin --output=%outbin% "%axfpath%"
exit 0