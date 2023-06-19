/*****************************************************************************
 *
 *    Copyright (c) 2016-2026 by Sophgo Technologies Inc. All rights reserved.
 *
 *    define bmrt_test structures used locally, and will not export to users
 *
 *****************************************************************************/

#ifndef __BMRT_TEST_INNER_H__
#define __BMRT_TEST_INNER_H__

#ifdef __linux__
#include <getopt.h>
#endif
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <sys/time.h>
#include <unistd.h>
#else
#include <windows.h>
#endif
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <thread>

#include "bmlib_runtime.h"
#include "bmodel.hpp"
#include "bmruntime.h"
#include "bmruntime.hpp"
#include "bmruntime_bmnet.h"
#include "bmruntime_cpp.h"
#include "bmruntime_interface.h"

using bmodel::ModelCtx;
using bmruntime::Bmruntime;
using std::max;
using std::min;
using std::set;
using std::string;
using std::vector;

#define INPUT_REF_DATA "input_ref_data.dat"
#define OUTPUT_REF_DATA "output_ref_data.dat"

extern vector<string> CONTEXT_DIR_V;
extern bool b_bmodel_dir;
extern string TEST_CASE;
extern int DEV_ID;
extern int LOOP_NUM;
extern int THREAD_NUM;
extern bool NEED_CMP;
extern uint64_t PREALLOC_SIZE;
extern void open_ref_file(const string &context_dir, FILE *&f_input, FILE *&f_output);
extern int result_cmp(int8_t **host_output_data, int8_t **ref_output_data, int output_num,
                      int *o_count, bm_data_type_t *o_dtype);
extern string fix_bmodel_path(const string& path);

#endif /* __BM_NET_H__ */
