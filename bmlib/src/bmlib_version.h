#ifndef _BMLIB_VERSION_H_
#define _BMLIB_VERSION_H_

#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif
#define COMMIT_HASH "xx"
#define BRANCH_NAME "xx"

#endif