#ifndef _BMLIB_VERSION_H_
#define _BMLIB_VERSION_H_

#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif
#define COMMIT_HASH "1531565aa4ab378b325d0a4bc87586c7c06c8af7"
#define BRANCH_NAME "v23.09-LTS"

#endif