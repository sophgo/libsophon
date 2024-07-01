#ifndef _BMLIB_VERSION_H_
#define _BMLIB_VERSION_H_

#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif
#define COMMIT_HASH "0ec89f7ab393194111c9cdfa0c9492e9d58e0894"
#define BRANCH_NAME "main"

#endif