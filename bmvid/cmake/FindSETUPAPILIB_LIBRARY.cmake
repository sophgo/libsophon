message(STATUS "now using SetupAPILib.cmake find SetupAPI Lib")

FIND_PATH(SETUPAPILIB_INCLUDE_DIR SetupAPI.h "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\um" "D:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\um"
"E:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\um" "D:\\Windows Kits\\10\\Include\\10.0.19041.0\\um")
message(STATUS "h dir ${SETUPAPILIB_INCLUDE_DIR}")

FIND_LIBRARY(SETUPAPILIB_LIBRARY SetupAPI.lib "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.19041.0\\um\\x64"  "D:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.19041.0\\um\\x64"
"E:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.19041.0\\um\\64" "D:\\Windows Kits\\10\\Lib\\10.0.19041.0\\um\\x64")
message(STATUS "lib dir: ${SETUPAPILIB_LIBRARY}")

if(SETUPAPILIB_INCLUDE_DIR AND SETUPAPILIB_LIBRARY)
  set(SETUPAPILIB_FOUND TRUE)
endif(SETUPAPILIB_INCLUDE_DIR AND SETUPAPILIB_LIBRARY)