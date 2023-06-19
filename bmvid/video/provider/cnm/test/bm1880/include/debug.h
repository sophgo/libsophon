#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>

void print_trace(void);
void hang(int ret);

#define ASSERT(_cond)                                   \
  do {                                                  \
    if (!(_cond)) {                                     \
      printf("ASSERT %s: %d: %s: %s\n",                 \
             __FILE__, __LINE__, __func__, #_cond);     \
      print_trace();                                    \
      fflush(stdout);                                   \
      hang(-1);                                         \
    }                                                   \
  } while(0)

#define FW_ERR(fmt, ...)           printf("FW_ERR: " fmt, ##__VA_ARGS__)
#define FW_INFO(fmt, ...)          printf("FW: " fmt, ##__VA_ARGS__)
#ifdef TRACE_FIRMWARE
#define FW_TRACE(fmt, ...)         printf("FW_TRACE: " fmt, ##__VA_ARGS__)
#else
#define FW_TRACE(fmt, ...)
#endif
#ifdef DEBUG_FIRMWARE
#define FW_DBG(fmt, ...)           printf("FW_DBG: " fmt, ##__VA_ARGS__)
#else
#define FW_DBG(fmt, ...)
#endif

#endif /* _DEBUG_H_ */
