/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <bmlib_runtime.h>

namespace faiss {
namespace tpu {
# if __WORDSIZE == 64
typedef long int		int64_t;
# else
__extension__
//typedef long long int		int64_t;
typedef long int		int64_t;
# endif

/// The metric space for vector comparison for Faiss indices and algorithms.
///
/// Most algorithms support both inner product and L2, with the flat
/// (brute-force) indices supporting additional metric types for vector
/// comparison.
enum MetricType {
	METRIC_INNER_PRODUCT = 0, ///< maximum inner product search
	METRIC_L2 = 1,            ///< squared L2 search
	METRIC_L1,                ///< L1 (aka cityblock)
};

enum {
	DT_INT8 = (0 << 1) | 1,
	DT_UINT8 = (0 << 1) | 0,
	DT_INT16 = (3 << 1) | 1,
	DT_UINT16 = (3 << 1) | 0,
	DT_FP16 = (1 << 1) | 1,
	DT_BFP16 = (5 << 1) | 1,
	DT_INT32 = (4 << 1) | 1,
	DT_UINT32 = (4 << 1) | 0,
	DT_FP32  = (2 << 1) | 1
} ;

class TpuIndex  {
	public:
	using idx_t = int64_t; ///< all indices are this type
	int d;        ///< vector dimension
	idx_t ntotal; ///< total nb of indexed vectors
	bool verbose; ///< verbosity level
	/// set if the Index does not require training, or if training is
	/// done already
	bool is_trained;
	/// type of metric this index uses for search
	MetricType metric_type;
	bool storeTransposed;
	bool useFloat16;

	explicit TpuIndex(
		bm_handle_t handle,
		int dims,
		faiss::tpu::MetricType metric,
		bool storeTransposed
		);

	~TpuIndex();

	void train(idx_t /*n*/, const float* /*x*/);

	/// Set the minimum data size for searches (in MiB) for which we use
	/// CPU -> TPU paging
	void setMinPagingSize(size_t size);

	/// Returns the current minimum data size for paged searches
	size_t getMinPagingSize() const;

	inline bm_handle_t get_bm_handle() {
		return m_handle;
	}

	/// `x` can be resident on the CPU or any TPU; copies are performed
	/// as needed
	/// Handles paged adds if the add set is too large; calls addInternal_
	void add(idx_t, const float* x);

	/// `x` and `ids` can be resident on the CPU or any TPU; copies are
	/// performed as needed
	/// Handles paged adds if the add set is too large; calls addInternal_
	void add_with_ids(idx_t n, const float* x, const idx_t* ids);

	/// `x`, `distances` and `labels` can be resident on the CPU or any
	/// TPU; copies are performed as needed
	void search(
		idx_t n,
		const float* x,
		idx_t k,
		float* distances,
		int* labels);

	void reset();

	protected:
	/// Copy what we need from the CPU equivalent
	void copyFrom(const faiss::tpu::TpuIndex* index);

	/// Copy what we have to the CPU equivalent
	void copyTo(faiss::tpu::TpuIndex* index) const;


	private:
	/// Handles paged adds if the add set is too large, passes to
	/// addImpl_ to actually perform the add for the current page
	void addPaged_(int n, const float* x, const idx_t* ids);

	/// Calls addImpl_ for a single page of TPU-resident data
	void addPage_(int n, const float* x, const idx_t* ids);

	void normsqrL2(int n, const float* input, float* output);


	protected:
	/// Size above which we page copies from the CPU to TPU
	size_t minPagedSize_;
	bm_handle_t m_handle;
	bm_device_mem_t m_base_mem;
	bm_device_mem_t m_base_norm;
	int64_t m_size_norm;
	int overflow_flag;
};

} // namespace tpu
} // namespace faiss
