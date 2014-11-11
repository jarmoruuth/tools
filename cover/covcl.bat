@echo off
rem Cover analyzer front end for Microsoft C. Compiles %1.c to %1.obj.

@echo on
cl %2 %3 %4 %5 %6 %7 %8 %9 -c -E %1.c > %1.i
@echo off
if errorlevel 1 goto error

@echo on
cover -v %1.i %1.cov %1.lst
@echo off
if errorlevel 1 goto error
del %1.i

@echo on
cl %2 %3 %4 %5 %6 %7 %8 %9 -c -Fo%1.obj -Tc%1.cov
@echo off
if errorlevel 1 goto error
del %1.cov

goto end

:error
echo Error in covcl compile
del %1.obj

:end
