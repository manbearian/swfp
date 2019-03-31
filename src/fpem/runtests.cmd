
@for %%x in (

 conv16to32.cpp
 basic_arith.cpp
 add16_all.cpp
 mul16_all.cpp
 div16_all.cpp

) do @(
 cl -nologo -EHsc -std:c++17 -Zi -W4 -diagnostics:caret -O2 -DNDEBUG %%~nx.cpp && %%~nx.exe
 if not errorlevel 0 goto :fail
 if errorlevel 1 goto :fail
)
echo PASS ALL TESTS
goto :eof


:fail
echo !!!FAILURE!!!
goto :eof

