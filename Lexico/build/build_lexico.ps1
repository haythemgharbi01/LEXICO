$ErrorActionPreference = "Stop"
Set-Location "c:\Users\hayth.DESKTOP-6LTO7A0\OneDrive\Desktop\Lexico"
$env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + $env:PATH

$CXX = "C:\msys64\mingw64\bin\g++.exe"
$INC = @("-Isrc", "-Ibuild", "-IC:\PROGRA~1\LLVM\include")
$CFLAGS = @("-std=c++17", "-Wall", "-Wextra", "-O2", "-DNDEBUG", "-ffunction-sections", "-fdata-sections", "-flto")

& $CXX @CFLAGS @INC -c src\main.cpp -o build\main.o
& $CXX @CFLAGS @INC -c src\driver.cpp -o build\driver.o
& $CXX @CFLAGS @INC -c src\ast.cpp -o build\ast.o
& $CXX @CFLAGS @INC -c src\symtab.cpp -o build\symtab.o
& $CXX @CFLAGS @INC -c src\codegen.cpp -o build\codegen.o
& $CXX @CFLAGS @INC -c src\lexico.tab.cpp -o build\lexico.tab.o
& $CXX @CFLAGS @INC -c src\lexico.lex.cpp -o build\lexico.lex.o

& $CXX build\main.o build\driver.o build\ast.o build\symtab.o build\codegen.o build\lexico.tab.o build\lexico.lex.o "-LC:\PROGRA~1\LLVM\lib" -lLLVM-C "-Wl,--gc-sections" -flto -o build\lexico.exe

Get-Item build\lexico.exe | Select-Object FullName, Length, LastWriteTime
