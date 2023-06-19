@echo off


cd ../
set bmvid_path=%cd%

cd ../
echo %cd%

cd ../
echo %bmvid_path%
if exist bm_prebuilt_toolchains_win (
  set bm_prebuilt_toolchains_win_path=%bmvid_path%\..\..\bm_prebuilt_toolchains_win\
  cd %bm_prebuilt_toolchains_win_path%
) else (
  git clone ssh://%git_name%@gerrit.ai.bitmaincorp.vip:29418/bm_prebuilt_toolchains_win
  echo %cd%
  set bm_prebuilt_toolchains_win_path=%bmvid_path%\..\bm_prebuilt_toolchains_win\
)

set path=%path%;%bm_prebuilt_toolchains_win_path%\cmake-3.20.0-windows-x86_64\bin;


cd %bmvid_path%

echo %bmvid_path%
set TARGET_PROJECT=bmvid_build

cmake . -B .\out\%TARGET_PROJECT% -G "Visual Studio 16 2019"
if %errorlevel% NEQ 0 (
  echo "cmake bmvid_build failed!"
  exit /B 1
) else (
  echo "cmake bmvid_build success!"
)

D:

cd C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE

devenv %bmvid_path%\out\%TARGET_PROJECT%\bmvid_win.sln /Build "Debug|x64"
if %errorlevel% NEQ 0 (
  echo "make bmvid_build failed!"
  exit /B 1
) else (
  echo "make bmvid_build success!"
)


xcopy /E /S /Y %bmvid_path%\projects\bm_sophon_winsdk\x64\Debug %bmvid_path%\out\release\DRIVER\
