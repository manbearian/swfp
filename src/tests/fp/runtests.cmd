
@for %%x in (

 conv16to32.cpp
 basic_arith.cpp
 basic_comp.cpp
 fp16_to_int_all.cpp
 int_to_fp16_all.cpp
 int_to_fp_misc.cpp
 add16_all.cpp
 mul16_all.cpp
 div16_all.cpp
 comp16_all.cpp

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

