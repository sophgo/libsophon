/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "TpuIndex.h"
#include <bmlib_runtime.h>
#include <bmcv_api_ext.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <FaissAssert.h>

namespace faiss {
namespace tpu {

/// Default CPU search size for which we use paged copies
constexpr size_t kMinPageSize = (size_t)256 * 1024 * 1024;

/// Size above which we page copies from the CPU to TPU (non-paged
/// memory usage)
constexpr size_t kNonPinnedPageSize = (size_t)256 * 1024 * 1024;

// Default size for which we page add or search
constexpr size_t kAddPageSize = (size_t)256 * 1024 * 1024;

// Or, maximum number of vectors to consider per page of add or search
constexpr size_t kAddVecSize = (size_t)512 * 1024;

// Use a smaller search size, as precomputed code usage on IVFPQ
// requires substantial amounts of memory
// FIXME: parameterize based on algorithm need
constexpr size_t kSearchVecSize = (size_t)32 * 1024;

#define BASE_MEMORY_LIMIT 2147483648//2*1024*1024*1024
#define NORM_MEMORY_LIMIT 8*1024*1024
#define DEV_TEMP_LIMIT 1*1024*1024*1024 + 20*1024*1024
#define INDEX_LIMIT 2*1024*1024
#define SEARCH_MEMORY_LIMIT 256*1024
#define TPU_MAX_SELECTION_K 1024

TpuIndex::TpuIndex(
	bm_handle_t handle,
	int dims,
	faiss::tpu::MetricType metric,
	bool storeTransposed
	)
	: d(dims),
	ntotal(0),
	verbose(false),
	is_trained(true),
	metric_type(metric),
	minPagedSize_(kMinPageSize),
	m_handle(handle),
	storeTransposed(storeTransposed),
	overflow_flag(0) {

	FAISS_THROW_IF_NOT_MSG(dims > 0, "Invalid number of dimensions");
	bm_status_t ret;
	ret = bm_malloc_device_byte(handle, &m_base_mem, BASE_MEMORY_LIMIT);
	FAISS_THROW_IF_NOT_MSG(ret == BM_SUCCESS, "malloc base_mem device failed");
	ret = bm_malloc_device_byte(handle, &m_base_norm, NORM_MEMORY_LIMIT);
	FAISS_THROW_IF_NOT_MSG(ret == BM_SUCCESS, "malloc base_norm device failed");
}

TpuIndex::~TpuIndex() {
	bm_free_device(m_handle,m_base_mem);
	bm_free_device(m_handle,m_base_norm);
	bm_dev_free(m_handle);
}

void TpuIndex::train(idx_t /*n*/, const float* /*x*/) {
	// does nothing by default
}

void TpuIndex::copyFrom(const faiss::tpu::TpuIndex* index) {
	d = index->d;
	metric_type = index->metric_type;
	ntotal = index->ntotal;
	is_trained = index->is_trained;
}

void TpuIndex::copyTo(faiss::tpu::TpuIndex* index) const {
	index->d = d;
	index->metric_type = metric_type;
	index->ntotal = ntotal;
	index->is_trained = is_trained;
}

void TpuIndex::setMinPagingSize(size_t size) {
	minPagedSize_ = size;
}

size_t TpuIndex::getMinPagingSize() const {
	return minPagedSize_;
}

void TpuIndex::add(idx_t n, const float* x) {
	// Pass to add_with_ids
	add_with_ids(n, x, nullptr);
}

void TpuIndex::add_with_ids(
	idx_t n,
	const float* x,
	const idx_t* ids) {

	FAISS_THROW_IF_NOT_MSG(this->is_trained, "Index not trained");
	// For now, only support <= max int results
	FAISS_THROW_IF_NOT_FMT(
		n <= (idx_t)std::numeric_limits<int>::max(),
		"TPU index only supports up to %d indices",
		std::numeric_limits<int>::max());

	if (n == 0) {
		// nothing to add
		return;
	}

	addPaged_((int)n, x, nullptr);
}

void TpuIndex::normsqrL2(int n, const float* input, float* output) {
	for(int i=0; i<n; i++)
		for(int j=0; j< this->d; j++)
			*(output + i) += (*(input + i * this->d + j)) * (*(input + i * this->d + j));

}

void TpuIndex::addPaged_(int n, const float* x, const idx_t* ids) {
	if (n > 0) {
		size_t totalSize = (size_t)n * this->d * sizeof(float);

		FAISS_THROW_IF_NOT_FMT(
		totalSize < BASE_MEMORY_LIMIT,
		"totalSize %d exceed limit",
		(int)totalSize);

		int64_t m_size_base = ntotal * this->d * sizeof(float);
		m_size_norm = m_size_base/this->d;

		if(m_size_base + totalSize > BASE_MEMORY_LIMIT){//cover the old database
			overflow_flag = 1;
			m_size_base = 0;
			m_size_norm = 0;
			printf("overflow, and start from base address\n");
		}

		bm_device_mem_t temp_mem = m_base_mem;
		temp_mem.size = totalSize;
		temp_mem.u.device.device_addr += m_size_base;
		int ret;
		ret = bm_memcpy_s2d(m_handle, temp_mem, (void *)x);
		FAISS_THROW_IF_NOT_FMT(
		ret == BM_SUCCESS,
		"bm_memcpy_s2d add failed ret= %d",
		ret);

		if(metric_type == 1){// L2 distance
			float* norm = (float*)malloc(n * sizeof(float));
			normsqrL2(n, x, norm);

			bm_device_mem_t temp_mem_norm = m_base_norm;
			temp_mem_norm.size = totalSize/d;
			temp_mem_norm.u.device.device_addr += m_size_norm;

			ret = bm_memcpy_s2d(m_handle, temp_mem_norm, (void *)norm);
			FAISS_THROW_IF_NOT_FMT(
			ret == BM_SUCCESS,
			"bm_memcpy_s2d norm failed ret= %d",
			ret);
			//or bmcv_normL2(...);//if use tpu to caculate L2norm
		}

		if(overflow_flag == 0) {
			ntotal+= n;// byte as a unit
		}
		m_size_norm = ntotal * sizeof(float); //bytes as a unit
		m_base_mem.size = ntotal * this->d * sizeof(float);;
		m_base_norm.size = m_size_norm;
		// Before continuing, we guarantee that all data will be resident on the TPU.
	}
}

void TpuIndex::search(
	idx_t n,
	const float* x,
	idx_t k,
	float* distances,
	int* labels) {

	FAISS_THROW_IF_NOT(k > 0);
	FAISS_THROW_IF_NOT_MSG(this->is_trained, "Index not trained");

	// For now, only support <= max int results
	FAISS_THROW_IF_NOT_FMT(
		n <= (idx_t)std::numeric_limits<int>::max(),
		"TPU index only supports up to %d indices",
		std::numeric_limits<int>::max());

	// Maximum k-selection supported is based on the CUDA SDK
	FAISS_THROW_IF_NOT_FMT(
		k <= TPU_MAX_SELECTION_K,
		"TPU index only supports k <= %d (requested %d)",
		TPU_MAX_SELECTION_K,
		(int)k); // select limitation

	FAISS_THROW_IF_NOT_FMT(
		n * k *4 <= INDEX_LIMIT,
		"TPU index only supports nq *k <= %d (requested %d)",
		INDEX_LIMIT,
		(int)(n*k * 4)); // query num limitation

	if (n == 0 || k == 0) {
		// nothing to search
		return;
	}

	// We guarantee that the bmcv search will be called with device-resident
	// pointers.

	// The input vectors may be too large for the TPU, but we still
	// assume that the output distances and labels are not.
	// Go ahead and make space for output distances and labels on the
	// TPU.
	// If we reach a point where all inputs are too big, we can add
	// another level of tiling.

	size_t totalSize = (size_t)n * this->d * sizeof(float);
	FAISS_THROW_IF_NOT_MSG(totalSize < SEARCH_MEMORY_LIMIT, "malloc search too large");
	int ret;
	bm_device_mem_t dev_distance;
	bm_device_mem_t dev_index;
	bm_device_mem_t dev_temp;
	bm_device_mem_t dev_xq;
	bm_device_mem_t dev_xq_l2;
	ret = bm_malloc_device_byte(m_handle, &dev_distance, n*k*4);
	FAISS_THROW_IF_NOT_MSG(ret == BM_SUCCESS, "malloc dev_distance memory failed");
	ret = bm_malloc_device_byte(m_handle, &dev_index, n*k*4);
	FAISS_THROW_IF_NOT_MSG(ret == BM_SUCCESS, "malloc dev_index memory failed");
	ret = bm_malloc_device_byte(m_handle, &dev_temp, DEV_TEMP_LIMIT);
	FAISS_THROW_IF_NOT_MSG(ret == BM_SUCCESS, "malloc dev_temp memory failed");
	ret = bm_malloc_device_byte(m_handle, &dev_xq, totalSize);
	FAISS_THROW_IF_NOT_MSG(ret == BM_SUCCESS, "malloc dev_xq memory failed");
	ret = bm_memcpy_s2d(m_handle, dev_xq, (void *)x);
	FAISS_THROW_IF_NOT_FMT(
		ret == BM_SUCCESS,
		"bm_memcpy_s2d xq failed ret= %d",
		ret);

	if(metric_type == 1){// L2 distance
		//caculate xq's L2norm
		float* normq = (float*)malloc(n * sizeof(float));
		normsqrL2(n, x, normq);
		ret = bm_malloc_device_byte(m_handle, &dev_xq_l2, 1024);
		FAISS_THROW_IF_NOT_MSG(ret == BM_SUCCESS, "malloc dev_xq l2norm memory failed");
		ret = bm_memcpy_s2d(m_handle, dev_xq_l2, (void *)normq);
		FAISS_THROW_IF_NOT_FMT(
			ret == BM_SUCCESS,
			"bm_memcpy_s2d xq_norm failed ret= %d",
			ret);
	}

	bool usePaged = false;

	if (!usePaged) {
		if(metric_type == 1)// L2 distance
			bmcv_faiss_indexflatL2(m_handle, dev_xq, m_base_mem, dev_xq_l2, m_base_norm, dev_temp, dev_distance, dev_index, this->d, n, ntotal, k, storeTransposed, DT_FP32, DT_FP32);
		else
			bmcv_faiss_indexflatIP(m_handle, dev_xq, m_base_mem, dev_temp, dev_distance, dev_index, this->d, n, ntotal, k, storeTransposed, DT_FP32, DT_FP32);
	}

	// Copy back if necessary
	ret = bm_memcpy_d2s(m_handle, (void *)distances, dev_distance);
	FAISS_THROW_IF_NOT_MSG(ret == BM_SUCCESS, "d2s dev_distance device failed");
	ret = bm_memcpy_d2s(m_handle, (void *)labels, dev_index);
	FAISS_THROW_IF_NOT_MSG(ret == BM_SUCCESS, "d2s dev_index device failed");

	bm_free_device(m_handle,dev_temp);
	bm_free_device(m_handle,dev_index);
	bm_free_device(m_handle,dev_distance);
	bm_free_device(m_handle,dev_xq);
	if(metric_type == 1){
		bm_free_device(m_handle,dev_xq_l2);
	}
}

void TpuIndex::reset() {
	ntotal = 0;
	m_size_norm = 0;
	overflow_flag = 0;
}

} // namespace tpu
} // namespace faiss
