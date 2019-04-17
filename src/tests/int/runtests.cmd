
@for %%x in (

  basic_init.cpp

  add16_all.cpp
  sub16_all.cpp
  mul16_all.cpp
  div16_all.cpp

) do @(
 pushd %tmp%
 cl -nologo -EHsc -std:c++17 -Zi -W4 -diagnostics:caret -O2 -DNDEBUG -I%%~px\..\.. %%~fx
 popd
 if not errorlevel 0 goto :fail
 if errorlevel 1 goto :fail
 %tmp%\%%~nx.exe
 if not errorlevel 0 goto :fail
 if errorlevel 1 goto :fail
)
echo PASS ALL TESTS
goto :eof


:fail
echo !!!FAILURE!!!
goto :eof

