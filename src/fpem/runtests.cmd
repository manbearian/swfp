
cl -nologo -EHsc -std:c++17 -Zi -W4 -diagnostics:caret -O2 -DNDEBUG conv16to32.cpp && conv16to32.exe
if not errorlevel 0 goto :fail
if errorlevel 1 goto :fail

cl -nologo -EHsc -std:c++17 -Zi -W4 -diagnostics:caret -O2 -DNDEBUG basic_arith.cpp && basic_arith.exe
if not errorlevel 0 goto :fail
if errorlevel 1 goto :fail
