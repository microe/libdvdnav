
ECHO mkdir install ...
rmdir /s install\include
rmdir /s install\lib
mkdir install\include\dvdnav
mkdir install\lib

ECHO includes ...
xcopy /Y ..\src\dvd_reader.h install\include\
xcopy /Y ..\src\ifo_types.h install\include\dvdnav
xcopy /Y ..\src\ifo_read.h install\include\dvdnav
xcopy /Y ..\src\dvdnav.h install\include\dvdnav

ECHO lib ...
xcopy /Y %1\libdvdnav\libdvdnav.lib install\lib
