@echo off
setlocal enabledelayedexpansion

echo ========================================
echo  SQLite 3.50.4 Complete Compilation Script (Windows)
echo  Includes: FTS5 + JSON1 + Custom Chinese Tokenizer
echo ========================================
echo.

REM Set paths
set SRC_DIR=src
set OUT_DIR=lib
set BIN_DIR=bin
set INCLUDE_DIR=include

REM Create directories
if not exist %OUT_DIR% mkdir %OUT_DIR%
if not exist %BIN_DIR% mkdir %BIN_DIR%
if not exist %INCLUDE_DIR% mkdir %INCLUDE_DIR%

REM Check necessary files
if not exist "%SRC_DIR%\sqlite3.c" (
    echo Error: Cannot find %SRC_DIR%\sqlite3.c
    echo Please download SQLite amalgamated source from https://sqlite.org/download.html
    echo and copy to %SRC_DIR%\ directory
    pause
    exit /b 1
)

if not exist "%SRC_DIR%\fts5_simple.c" (
    echo Error: Cannot find %SRC_DIR%\fts5_simple.c
    echo Please ensure custom tokenizer source exists
    pause
    exit /b 1
)

echo Step 1: Compiling SQLite core...
echo ========================================

REM Compile SQLite core (3.50.4 version recommended options)
cl /c /O2 /Zi /nologo ^
  /DSQLITE_ENABLE_FTS5 ^
  /DSQLITE_ENABLE_JSON1 ^
  /DSQLITE_ENABLE_RTREE ^
  /DSQLITE_ENABLE_COLUMN_METADATA ^
  /DSQLITE_ENABLE_PREUPDATE_HOOK ^
  /DSQLITE_ENABLE_SESSION ^
  /DSQLITE_ENABLE_RBU ^
  /DSQLITE_ENABLE_CARRAY ^
  /DSQLITE_THREADSAFE=1 ^
  /DSQLITE_DEFAULT_MEMSTATUS=0 ^
  /DSQLITE_OMIT_PROXY_LOCKING ^
  /DSQLITE_USE_URI=1 ^
  /DSQLITE_OS_WIN=1 ^
  /I%SRC_DIR% ^
  "%SRC_DIR%\sqlite3.c" ^
  /Fo"%OUT_DIR%\sqlite3.obj"

if errorlevel 1 (
    echo ? SQLite compilation failed
    pause
    exit /b 1
)
echo ? SQLite compilation successful

echo.
echo Step 2: Compiling custom tokenizer...
echo ========================================

REM Compile custom tokenizer
cl /c /O2 /Zi /nologo ^
  /DSQLITE_ENABLE_FTS5 ^
  /DSQLITE_CORE=1 ^
  /DSQLITE_OMIT_PROXY_LOCKING ^
  /I%SRC_DIR% ^
  "%SRC_DIR%\fts5_simple.c" ^
  /Fo"%OUT_DIR%\fts5_simple.obj"

if errorlevel 1 (
    echo ? Tokenizer compilation failed
    pause
    exit /b 1
)
echo ? Tokenizer compilation successful

echo.
echo Step 3: Generating static library...
echo ========================================

REM Generate static library
lib /NOLOGO ^
  /OUT:"%OUT_DIR%\sqlite3_fts5_simple.lib" ^
  "%OUT_DIR%\sqlite3.obj" ^
  "%OUT_DIR%\fts5_simple.obj"

if errorlevel 1 (
    echo ? Static library generation failed
    pause
    exit /b 1
)
echo ? Static library generated: %OUT_DIR%\sqlite3_fts5_simple.lib

echo.
echo Step 4: Generating dynamic library (DLL)...
echo ========================================

REM Generate dynamic library
link /DLL /NOLOGO ^
  /OUT:"%OUT_DIR%\sqlite3_fts5_simple.dll" ^
  /IMPLIB:"%OUT_DIR%\sqlite3_fts5_simple_dll.lib" ^
  /DEF:"%SRC_DIR%\sqlite3.def" ^
  "%OUT_DIR%\sqlite3.obj" ^
  "%OUT_DIR%\fts5_simple.obj" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\kernel32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\user32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\gdi32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\winspool.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\comdlg32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\advapi32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\shell32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\ole32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\oleaut32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\odbc32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\odbccp32.lib" ^
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\Uuid.Lib" ^
  oldnames.lib

if errorlevel 1 (
    echo ??  DLL generation failed, but static library is available
) else (
    echo ? Dynamic library generated: %OUT_DIR%\sqlite3_fts5_simple.dll
)

echo.
echo Step 5: Copying header files...
echo ========================================

REM Copy header files
copy "%SRC_DIR%\sqlite3.h" "%INCLUDE_DIR%\" >nul
copy "%SRC_DIR%\fts5_simple.h" "%INCLUDE_DIR%\" >nul
copy "%SRC_DIR%\sqlite3ext.h" "%INCLUDE_DIR%\" >nul
echo ? Header files copied to %INCLUDE_DIR%\ directory

echo.
echo Step 6: Compiling test program...
echo ========================================

REM Compile test program
if exist "test\test_simple.c" (
    cl /O2 /Zi /nologo /NODEFAULTLIB ^
      /I%INCLUDE_DIR% ^
      "test\test_simple.c" ^
      "%OUT_DIR%\sqlite3_fts5_simple.lib" ^
      "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\Uuid.Lib" ^
      "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\kernel32.lib" ^
      "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\user32.lib" ^
      "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\um\x86\advapi32.lib" ^
        msvcrt.lib ^
        oldnames.lib ^
        /Fe"%BIN_DIR%\test_simple.exe"
    
    if errorlevel 1 (
        echo ? Test program compilation failed
    ) else (
        echo ? Test program generated: %BIN_DIR%\test_simple.exe
    )
)

echo.
echo ========================================
echo Compilation completed!
echo ========================================
echo.
echo ? Output files:
dir /b "%OUT_DIR%\*"
echo.
echo ? Run test:
if exist "%BIN_DIR%\test_simple.exe" (
    echo   %BIN_DIR%\test_simple.exe
)
echo.
pause