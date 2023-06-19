@echo off
::------------------------------set compile and runtimelib type-----------------------------------
echo %cd%
cd ..
set test_path=%cd%
set param=%1
set type=%2
set runtime_lib=%3
set enable_jenkins=%4
if "%type%"=="" (
  set compile_type="Debug|x64"
  echo %runtime_lib%| findstr MT >nul && (
    set runtime_type=MT
  ) || (
    set runtime_type=MD
  )
) else if "%type%"=="debug" (
  set compile_type="Debug|x64"
  echo %runtime_lib%| findstr MT >nul && (
    set runtime_type=MT
  ) || (
    set runtime_type=MD
  )
) else if "%type%"=="release" (
  set compile_type="Release|x64"
  echo %runtime_lib%| findstr MT >nul && (
    set runtime_type=MT
  ) || (
    set runtime_type=MD
  )
) else (
  echo input compile type error! Please input debug/release!
  exit /B 1
)

::---------------------------------------set environment----------------------------------------
FOR /F "tokens=* USEBACKQ" %%F IN (`git config user.name`) DO (
  SET git_name=%%F
)
if "%enable_jenkins%"=="" (
  set enable_jenkins=0
  set check_path=%test_path%\..\..\
) else (
  set enable_jenkins=1
  set check_path=%test_path%\..\
)
cd %check_path%

if exist bm_prebuilt_toolchains_win (
  set bm_prebuilt_toolchains_win_path=%check_path%\bm_prebuilt_toolchains_win
  cd %bm_prebuilt_toolchains_win_path%
) else (
  git clone ssh://%git_name%@gerrit-ai.sophgo.vip:29418/bm_prebuilt_toolchains_win
  echo %cd%
  set bm_prebuilt_toolchains_win_path=%check_path%\bm_prebuilt_toolchains_win
)

set path=%path%;%bm_prebuilt_toolchains_win_path%\cmake-3.20.0-windows-x86_64\bin;


::---------------------------------------set make module----------------------------------------
if "%param%"=="make_bmvid" (
  echo %compile_type%
  call :make_bmvid
  if errorlevel 1 set ret_value=1
) else if "%param%"=="make_clean" (
  call :make_clean
  if errorlevel 1 set ret_value=1
) else if "%param%"=="" (
  call :help
  if errorlevel 1 set ret_value=1
) else (
  echo input param error! Please input with usage help!
  cd %test_path%\build
  exit /B 1
)
exit /b %ret_value%

::------------------------------------------help------------------------------------------------
:help
echo envsetup_win.bat usage:
echo make_bmvid: build vpu,jpu,vpp in bmvid.
echo make_clean: clean all in output path.
cd %test_path%\build
goto:eof


:make_bmvid
cd %test_path%

echo %test_path%
set TARGET_PROJECT=bmvid_build

echo %compile_type% | findstr Debug >nul
if errorlevel 1 (
  echo release
  cmake . -B .\out\%TARGET_PROJECT% -G "Visual Studio 16 2019" -DTARGET_TYPE=release -DRUNTIME_LIB=%runtime_type%
) else (
  echo debug
  cmake . -B .\out\%TARGET_PROJECT% -G "Visual Studio 16 2019" -DTARGET_TYPE=debug -DRUNTIME_LIB=%runtime_type%
)
if errorlevel 1 (
  echo "cmake bmvid_build failed!"
  if %enable_jenkins% NEQ 0 (
    exit 1
  ) else (
    exit /B 1
  )
) else (
  echo "cmake bmvid_build success!"
)

devenv %test_path%\out\%TARGET_PROJECT%\bmvid_win.sln /Build %compile_type%
if errorlevel 1 (
  echo "make bmvid_build failed!"
  if %enable_jenkins% NEQ 0 (
    exit 1
  ) else (
    exit /B 1
  )
) else (
  echo "make bmvid_build success!"
)

cd %test_path%\build
goto:eof

:make_clean
cd %test_path%
rd /q /s .\out
cd %test_path%\build
goto:eof
