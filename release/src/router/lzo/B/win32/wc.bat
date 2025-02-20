@echo // Copyright (C) 1996-2015 Markus F.X.J. Oberhumer
@echo //
@echo //   Windows 32-bit
@echo //   Watcom C/C++
@echo //
@call b\prepare.bat
@if "%BECHO%"=="n" echo off


set CC=wcl386 -zq -mf -5r -bt#nt -l#nt
set CF=-ox -zc %CFI% %CFASM%
set LF=%BLIB%

%CC% %CF% -c src\*.c
@if errorlevel 1 goto error
wlib -q -b -n -t %BLIB% @b\win32\wc.rsp
@if errorlevel 1 goto error

%CC% %CF% examples\dict.c %LF%
@if errorlevel 1 goto error
%CC% %CF% examples\lzopack.c %LF%
@if errorlevel 1 goto error
%CC% %CF% examples\precomp.c %LF%
@if errorlevel 1 goto error
%CC% %CF% examples\precomp2.c %LF%
@if errorlevel 1 goto error
%CC% %CF% examples\simple.c %LF%
@if errorlevel 1 goto error

%CC% %CF% lzotest\lzotest.c %LF%
@if errorlevel 1 goto error


@call b\done.bat
@goto end
:error
@echo ERROR during build!
:end
@call b\unset.bat
