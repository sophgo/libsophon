#ifndef __W5_DEC_TEST_H__
#define __W5_DEC_TEST_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef BOOL
typedef int BOOL;
#endif

typedef struct _TestDecConfig TestDecConfig;

BOOL TestDecoder(
    TestDecConfig *param
    );


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif
