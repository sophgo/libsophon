/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdio>
#include <cstdlib>
#include <random>

#include "../TpuIndex.h"
#include <bmlib_runtime.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/time.h>

using namespace std;
#define transpose 1 // right matrix shape  when 0: d * nb, when 1: nb * d
//64-bit int
using idx_t = faiss::tpu::TpuIndex::idx_t;

int main() {
int d = 256;      // dimension
idx_t nb = 2000000; // database size
int nq = 1;  // nb of queries

struct timeval search_start;
struct timeval search_end;

std::mt19937 rng;
std::uniform_real_distribution<> distrib;

float* xb = new float[d * nb];
float* xq = new float[d * nq];

if (transpose) {
	for (int i = 0; i < nb; i++) {
		for (int j = 0; j < d; j++)
		xb[d * i + j] = distrib(rng);
		xb[d * i] += i / 1000.;
	}
} else {
	for (int i = 0; i < d; i++) {
		for (int j = 0; j < nb; j++)
		xb[nb * i + j] = distrib(rng);
		xb[nb * i] += i / 1000.;
	}
}

for (int i = 0; i < nq; i++) {
	for (int j = 0; j < d; j++)
	xq[d * i + j] = distrib(rng);
	xq[d * i] += i / 1000.;
}

bm_status_t bm_ret = BM_SUCCESS;
bm_handle_t bm_handle = NULL;

bm_ret = bm_dev_request(&bm_handle, 0);
if (bm_ret != BM_SUCCESS) {
	printf("create device handle failed\n");
	return -1;
}

//faiss::tpu::TpuIndex index(bm_handle, d, (faiss::tpu::MetricType)0, transpose);//0 inter PRODUCT; 1 L2
faiss::tpu::TpuIndex index(bm_handle, d, (faiss::tpu::MetricType)1, transpose);//0 inter PRODUCT; 1 L2
printf("is_trained = %s\n", index.is_trained ? "true" : "false");
gettimeofday(&search_start, NULL);
index.add(nb, xb); // add vectors to the index
gettimeofday(&search_end, NULL);
int64_t delta = search_end.tv_sec * 1000000 + search_end.tv_usec - search_start.tv_sec * 1000000 - search_start.tv_usec;
printf("add time = %ld us\n", delta);
printf("ntotal = %zd\n", index.ntotal);

int k = 100;

{ // search xq
	//idx_t* I = new long[k * nq];
	int* I = new int[k * nq];
	float* D = new float[k * nq];

	gettimeofday(&search_start, NULL);
	index.search(nq, xq, k, D, I);
	gettimeofday(&search_end, NULL);
	delta = search_end.tv_sec * 1000000 + search_end.tv_usec - search_start.tv_sec * 1000000 - search_start.tv_usec;
	printf("search time = %ld us\n", delta);

	// print results
	printf("I (nq =%d first results, k= %d)\n", nq, k);
	for (int i = 0; i < nq; i++) {
	for (int j = 0; j < k; j++)
		printf("%5zd ", I[i * k + j]);
	printf("\n");
	}

	printf("D=\n");
	for (int i = 0; i < nq; i++) {
	for (int j = 0; j < k; j++)
		printf("%7g ", D[i * k + j]);
	printf("\n");
	}

	delete[] I;
	delete[] D;
}
delete[] xb;
delete[] xq;

return 0;
}
