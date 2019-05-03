@if not defined _ECHO echo off
setlocal

set single_test=%1
if defined single_test (
  call :run_test %single_test%
  if errorlevel 1 goto :fail
  goto :eof
)
  

for %%x in (

  basic_init.cpp

  neg16_all.cpp
  shift16_all.cpp

  add16_all.cpp
  sub16_all.cpp
  mul16_all.cpp
  div16_all.cpp

  addc16_all.cpp
  sub16_all.cpp
  mulext16_all.cpp

) do (
 call :run_test %%x
 if errorlevel 1 goto :fail
)
echo PASS ALL TESTS
goto :eof

::::::::::::::::::::::::::::::::::::
:run_test
:  %1 - name of test
::::::::::::::::::::::::::::::::::::
 cl -nologo -EHsc -std:c++17 -Zi -W4 -diagnostics:caret -O2 -DNDEBUG -I%~p1\..\.. %~f1
 popd
 if not errorlevel 0 exit /b 1
 if errorlevel 1 exit /b 1
 %tmp%\%~n1.exe
 if not errorlevel 0 exit /b 1
 if errorlevel 1 exit /b 1
 exit /b 0


:fail
echo !!!FAILURE!!!
goto :eof

