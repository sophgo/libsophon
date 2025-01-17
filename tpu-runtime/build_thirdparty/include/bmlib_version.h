#ifndef _BMLIB_VERSION_H_
#define _BMLIB_VERSION_H_

#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif
#define COMMIT_HASH "06c7ffee1623b4cb97980092c80241b4d96078d8"
#define BRANCH_NAME "main"

#endif