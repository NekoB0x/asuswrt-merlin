@echo // Copyright (C) 1996-2015 Markus F.X.J. Oberhumer
@echo //
@echo //   DOS 32-bit
@echo //   Microway NDP C/C++
@echo //
@call b\prepare.bat
@if "%BECHO%"=="n" echo off


set CC=mx486
set CF=-ansi -on %CFI%
set LF=%BLIB% -bind -map

@REM %CC% %CF% -Isrc -c src\*.c
for %%f in (src\*.c) do %CC% %CF% -Isrc -c %%f
@if errorlevel 1 goto error
ndplib %BLIB% @b\dos32\ndp.rsp
@if errorlevel 1 goto error

%CC% %CF% -Iexamples examples\dict.c %LF%
@if errorlevel 1 goto error
%CC% %CF% -Iexamples examples\lzopack.c %LF%
@if errorlevel 1 goto error
%CC% %CF% -Iexamples examples\precomp.c %LF%
@if errorlevel 1 goto error
%CC% %CF% -Iexamples examples\precomp2.c %LF%
@if errorlevel 1 goto error
%CC% %CF% -Iexamples examples\simple.c %LF%
@if errorlevel 1 goto error

%CC% %CF% -Dconst= -Ilzotest lzotest\lzotest.c %LF%
@if errorlevel 1 goto error


@call b\done.bat
@goto end
:error
@echo ERROR during build!
:end
@call b\unset.bat
