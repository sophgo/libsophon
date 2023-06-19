#ifndef _USER_BMCPU_MACRO_H_
#define _USER_BMCPU_MACRO_H_

#include <stdio.h>
#include <vector>
#include <map>
#include <string>
#include <set>
#include <list>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <math.h>

using std::vector;
using std::map;
using std::pair;
using std::make_pair;
using std::string;
using std::cout;
using std::endl;
using std::set;
using std::list;

#include <memory>
#define USER_STD  std

#define USER_ASSERT(_cond)                       \
  do {                                           \
    if (!(_cond)) {                              \
      printf("ASSERT %s: %s: %d: %s\n",          \
          __FILE__, __func__, __LINE__, #_cond); \
      exit(-1);                                  \
    }                                            \
  } while(0)


#endif
