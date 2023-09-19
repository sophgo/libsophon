@ECHO OFF

SET target_type=%1
SET runtime_type=%2
SET build_task=%3

SET path_bk=%path%
SET bm_prebuilt_toolchains_win_path=%cd%\..\bm_prebuilt_toolchains_win
SET path=%path%;%bm_prebuilt_toolchains_win_path%\cmake-3.20.0-windows-x86_64\bin
SET path=%path%;%bm_prebuilt_toolchains_win_path%\mingw64\bin
SET path=%path%;%bm_prebuilt_toolchains_win_path%\gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu\bin

SET cwd=%cd%

IF NOT "%target_type%"=="release" IF NOT "%target_type%"=="debug" (
    CALL :usage
)

IF NOT "%runtime_type%"=="MT" IF NOT "%runtime_type%"=="MD" (
    CALL :usage
)

IF "%target_type%"=="debug" (
  SET compile_type="Debug|x64"
) ELSE (
  SET compile_type="Release|x64"
)

IF "%build_task%"=="" (
    SET build_task=all
)

IF "%build_task%"=="all" (
    CALL :build_module .\ libsophon
) ELSE IF "%build_task%"=="bmlib" (
    CALL :build_module .\bmlib bmlib
) ELSE IF "%build_task%"=="bm-smi" (
    CALL :build_module .\bm-smi bm-smi
) ELSE IF "%build_task%"=="tpu-bmodel" (
    CALL :build_module .\tpu-bmodel tpu-bmodel
) ELSE IF "%build_task%"=="tpu-cpuop" (
    CALL :build_module .\tpu-cpuop cpu-ops
) ELSE IF "%build_task%"=="tpu-runtime" (
    CALL :build_module .\tpu-runtime tpu-runtime
) ELSE IF "%build_task%"=="bmvid" (
    CALL :build_module .\bmvid bmvid_win bmvid_cmake
) ELSE IF "%build_task%"=="pack" (
    CALL :pack_libsophon
) ELSE (
    CALL :usage
)

SET path=%path_bk%

GOTO:EOF

:build_module

SET entry_dir=%1
SET project=%2
SET build_dir=%3
IF "%build_dir%"=="" (
    SET build_dir=build
)

PUSHD %entry_dir%

cmake . -B .\%build_dir% -G "Visual Studio 16 2019" -DTARGET_TYPE=%target_type% -DRUNTIME_LIB=%runtime_type%
CALL :check_ret Failed to generate the build system of %project%

devenv .\%build_dir%\%project%.sln /Build %compile_type%
CALL :check_ret Failed to build %project%

POPD

GOTO:EOF

:pack_libsophon

devenv %cwd%\build\libsophon.sln /Build %compile_type% /Project %cwd%\build\PACK.vcxproj
CALL:check_ret Failed to pack libsophon

GOTO:EOF

::------------------------------------ utils ------------------------------------

:check_ret
IF ERRORLEVEL 1 (
    SET err_msg=%*
    IF NOT "%err_msg%"=="" ( ECHO %err_msg% )
    CALL :err_exit
)
GOTO:EOF

:usage
ECHO "Usage:                                                             "
ECHO "  call build.bat <compile_type> <runtime_type> [module]            "
ECHO "                                                                   "
ECHO "  compile_type : release | debug                                   "
ECHO "  runtime_type : MT      | MD                                      "
ECHO "  module       :                                                   "
ECHO "    all        : default, build all modules                        "
ECHO "    bmlib      : build bmlib                                       "
ECHO "    bm-smi     : build bm-smi, depends on bmlib                    "
ECHO "    tpu-bmodel : build tpu-bmodel                                  "
ECHO "    tpu-cpuop  : build tpu-cpuop                                   "
ECHO "    tpu-runtime: build tpu-runtime, depends on bmlib               "
ECHO "    bmvid      : build bmvid(jpu, vpu, vpp, bmcv), depends on bmlib"
ECHO "                                                                   "
ECHO "    pack       : pack libsophon to libsophon_win_x.x.x_${arch}.zip "
CALL :err_exit
GOTO:EOF

:err_exit
CD %cwd%
SET path=%path_bk%
EXIT 1
GOTO:EOF