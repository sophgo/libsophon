#include "bmcv_api_ext.h"
#include <iostream>
#include <random>
#include <memory>
#include <assert.h>
#include <sys/time.h>

using namespace std;

#define TEST_WITH_FAISS 0
#define BMLIB_SAFE_CALL(cmd) assert(cmd == BM_SUCCESS)

#if TEST_WITH_FAISS
#include <faiss/IndexPQ.h>
#include <faiss/IndexFlat.h>
#include <faiss/utils/distances.h>
using idx_t = faiss::Index::idx_t;
using MetricType = faiss::MetricType;

bm_status_t compare_result_with_faiss_ADC(
    int ny,
    int nx,
    int d,
    int m,
    int ksub,
    int dsub,
    int k,
    int seed,
    int IP_metric)
{
    bm_handle_t handle;
    bm_status_t ret = BM_SUCCESS;
    ret = bm_dev_request(&handle, 0);
    if (BM_SUCCESS != ret)
    {
        std::cout << "request dev failed" << std::endl;
        return BM_ERR_FAILURE;
    }

    /// input
    float *nxquery_input_sys = new float[nx * d];
    float *database_input_sys = new float[ny * d];
    float *centroids_input_sys = new float[m * ksub * dsub];
    unsigned char *nycodes_input_sys = new unsigned char[ny * m];
    /// output
    float *fs_distance_output = new float[nx * k];
    idx_t *fs_index_output = new idx_t[nx * k];
    float *distance_output_sys = new float[nx * ny];
    int *index_output_sys = new int[nx * ny];

    /// fs index
    idx_t ntotal = ny;
    size_t nbits = 8;
    size_t M = m;
    size_t ks = ksub;
    size_t ds = dsub;

    std::cout << "-----------train faiss----------\n"
              << std::endl;
    MetricType metric = IP_metric ? MetricType::METRIC_INNER_PRODUCT : MetricType::METRIC_L2;
    faiss::IndexPQ index(m * dsub, M, nbits, metric);
    index.pq.verbose = 1; // training log
    // index.pq.cp.spherical = true; //normalize centroids
    index.train(ntotal, database_input_sys);
    index.reset();
    index.add(ntotal, database_input_sys);

    std::cout << "------------set input-----------" << std::endl;
    std::mt19937 rng;
    rng.seed(seed);
    std::uniform_real_distribution<float> dist_value(-10, 10);
    for (int i = 0; i < nx; i++)
    {
        for (int j = 0; j < d; j++)
        {
            nxquery_input_sys[i * d + j] = dist_value(rng);
        }
    }
    // faiss::fvec_renorm_L2(d, 1, query_input_sys);         //normalize query
    std::cout << "set query, done" << std::endl;
    for (int i = 0; i < ny; i++)
    {
        for (int j = 0; j < d; j++)
        {
            database_input_sys[i * d + j] = dist_value(rng);
        }
    }
    std::cout << "set database, done" << std::endl;
    for (size_t i = 0; i < M; i++)
    {
        for (size_t j = 0; j < ks; j++)
        {
            for (size_t k = 0; k < ds; k++)
            {
                centroids_input_sys[(i * ksub + j) * dsub + k] = index.pq.get_centroids(i, j)[k];
            }
        }
    }
    std::cout << "set centroids, done" << std::endl;
    for (int i = 0; i < ny; i++)
    {
        for (int j = 0; j < m; j++)
        {
            nycodes_input_sys[i * m + j] = index.codes[i * m + j];
            // nycodes_input_sys[j*n+i] = index.codes[i*m+j];         //nc_trans
        }
    }
    std::cout << "set datacodes, done" << std::endl;
    std::cout << "-----------faiss search----------" << std::endl;
    index.search(nx, nxquery_input_sys, k, fs_distance_output, fs_index_output);

    bm_device_mem_t centroids_input_dev;
    bm_device_mem_t nxquery_input_dev;
    bm_device_mem_t nycodes_input_dev;
    bm_device_mem_t distance_output_dev;
    bm_device_mem_t index_output_dev;

    int centroids_size = m * ksub * dsub * sizeof(float);
    int nxquery_size = nx * d * sizeof(float);
    int nycodes_size = ny * m * sizeof(char);
    int distance_size = nx * ny * sizeof(float);
    int index_size = nx * ny * sizeof(int);

    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &centroids_input_dev, centroids_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &nxquery_input_dev, nxquery_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &nycodes_input_dev, nycodes_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &distance_output_dev, distance_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &index_output_dev, index_size));

    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, centroids_input_dev, centroids_input_sys));
    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, nxquery_input_dev, nxquery_input_sys));
    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, nycodes_input_dev, nycodes_input_sys));

    std::cout << "------------tpu search----------" << std::endl;
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    bmcv_faiss_indexPQ_ADC(handle,
                           centroids_input_dev,
                           nxquery_input_dev,
                           nycodes_input_dev,
                           distance_output_dev,
                           index_output_dev,
                           ny, nx, d, m, ksub, k, IP_metric);
    gettimeofday(&t2, NULL);
    std::cout << "TPU using time(us): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)" << std::endl;
    std::cout << "TPU using time(ms): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / 1000 << "(ms)" << std::endl;

    BMLIB_SAFE_CALL(bm_memcpy_d2s(handle, distance_output_sys, distance_output_dev));
    BMLIB_SAFE_CALL(bm_memcpy_d2s(handle, index_output_sys, index_output_dev));

    std::cout << "ADCsearch result:" << std::endl;
    for (int i = 0; i < k; i++)
    {
        std::cout << "top:" << i + 1 << std::endl;
        std::cout << "tpu:   index:" << index_output_sys[i] << "\t";
        std::cout << "distance:" << distance_output_sys[i];
        std::cout << std::endl;
        std::cout << "faiss: index:" << index_output_sys[i] << "\t";
        std::cout << "distance:" << distance_output_sys[i];
        std::cout << std::endl;
    }

    bm_free_device(handle, centroids_input_dev);
    bm_free_device(handle, nxquery_input_dev);
    bm_free_device(handle, nycodes_input_dev);
    bm_free_device(handle, distance_output_dev);
    bm_free_device(handle, index_output_dev);

    delete[] database_input_sys;
    delete[] centroids_input_sys;
    delete[] nxquery_input_sys;
    delete[] nycodes_input_sys;
    delete[] fs_distance_output;
    delete[] fs_index_output;
    delete[] distance_output_sys;
    delete[] index_output_sys;

    bm_dev_free(handle);
    return BM_SUCCESS;
}

bm_status_t compare_result_with_faiss_SDC(
    int ny,
    int nx,
    int d,
    int m,
    int ksub,
    int k,
    int seed,
    int IP_metric)
{
    bm_handle_t handle;
    bm_status_t ret = BM_SUCCESS;
    ret = bm_dev_request(&handle, 0);
    if (BM_SUCCESS != ret)
    {
        std::cout << "request dev failed" << std::endl;
        return BM_ERR_FAILURE;
    }

    /// input
    float *query_input_sys = new float[nx * d];
    float *database_input_sys = new float[ny * d];
    float *sdc_table_input_sys = new float[m * ksub * ksub];
    unsigned char *nxcodes_input_sys = new unsigned char[nx * m];
    unsigned char *nycodes_input_sys = new unsigned char[ny * m];

    /// output
    uint8_t *fs_query_codes = new uint8_t[nx * m];
    float *fs_distance_output = new float[nx * k];
    idx_t *fs_index_output = new idx_t[nx * k];
    float *distance_output_sys = new float[nx * ny];
    int *index_output_sys = new int[nx * ny];

    /// fs index
    idx_t ntotal = ny;
    size_t nbits = 8;
    size_t M = m;
    size_t ks = ksub;

    std::cout << "-----------train faiss----------\n"
              << std::endl;
    MetricType metric = IP_metric ? MetricType::METRIC_INNER_PRODUCT : MetricType::METRIC_L2;
    faiss::IndexPQ index(d, M, nbits, metric);
    index.pq.verbose = 1; // training log
    // index.pq.cp.spherical = true;               //normalize centroids
    index.train(ntotal, database_input_sys);
    index.reset();
    index.add(ntotal, database_input_sys);

    index.pq.compute_codes(query_input_sys, fs_query_codes, nx);
    index.pq.compute_sdc_table();

    std::cout << "------------set input-----------\n"
              << std::endl;
    std::mt19937 rng;
    rng.seed(seed);
    std::uniform_real_distribution<float> dist_value(-10, 10);
    for (int i = 0; i < nx; i++)
    {
        for (int j = 0; j < d; j++)
        {
            query_input_sys[i * d + j] = dist_value(rng);
        }
    }
    // faiss::fvec_renorm_L2(d, 1, query_input_sys);         //normalize query
    std::cout << "set query, done\n"
              << std::endl;
    for (int i = 0; i < ny; i++)
    {
        for (int j = 0; j < d; j++)
        {
            database_input_sys[i * d + j] = dist_value(rng);
        }
    }
    std::cout << "set database, done\n"
              << std::endl;
    for (size_t i = 0; i < M; i++)
    {
        for (size_t j = 0; j < ks; j++)
        {
            for (size_t k = 0; k < ks; k++)
            {
                sdc_table_input_sys[(i * ksub + j) * ksub + k] = index.pq.sdc_table[(i * ksub + j) * ksub + k];
            }
        }
    }
    std::cout << "set sdc_table, done\n"
              << std::endl;
    for (int i = 0; i < ny; i++)
    {
        for (int j = 0; j < m; j++)
        {
            nycodes_input_sys[i * m + j] = index.codes[i * m + j];
        }
    }
    std::cout << "set datacodes, done\n"
              << std::endl;
    std::cout << "-----------faiss search----------\n"
              << std::endl;
    index.search(nx, query_input_sys, k, fs_distance_output, fs_index_output);

    bm_device_mem_t sdc_table_input_dev;
    bm_device_mem_t nxcodes_input_dev;
    bm_device_mem_t nycodes_input_dev;
    bm_device_mem_t distance_output_dev;
    bm_device_mem_t index_output_dev;

    int sdc_table_size = m * ksub * ksub * sizeof(float);
    int nxcodes_size = nx * m * sizeof(char);
    int nycodes_size = ny * m * sizeof(char);
    int output_distance_size = nx * ny * sizeof(float);
    int output_index_size = nx * ny * sizeof(int);

    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &sdc_table_input_dev, sdc_table_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &nxcodes_input_dev, nxcodes_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &nycodes_input_dev, nycodes_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &distance_output_dev, output_distance_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &index_output_dev, output_index_size));

    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, sdc_table_input_dev, sdc_table_input_sys));
    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, nxcodes_input_dev, nxcodes_input_sys));
    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, nycodes_input_dev, nycodes_input_sys));

    std::cout << "------------tpu search----------\n"
              << std::endl;
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    bmcv_faiss_indexPQ_SDC(handle,
                           sdc_table_input_dev,
                           nxcodes_input_dev,
                           nycodes_input_dev,
                           distance_output_dev,
                           index_output_dev,
                           ny, nx, m, ksub, k, IP_metric);
    gettimeofday(&t2, NULL);
    std::cout << "TPU using time(us): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)" << std::endl;
    std::cout << "TPU using time(ms): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / 1000 << "(ms)" << std::endl;

    BMLIB_SAFE_CALL(bm_memcpy_d2s(handle, distance_output_sys, distance_output_dev));
    BMLIB_SAFE_CALL(bm_memcpy_d2s(handle, index_output_sys, index_output_dev));

    std::cout << "SDCsearch result:" << std::endl;
    for (int i = 0; i < k; i++)
    {
        std::cout << "top:" << i + 1 << std::endl;
        std::cout << "tpu:   index:" << index_output_sys[i] << "\t";
        std::cout << "distance:" << distance_output_sys[i];
        std::cout << std::endl;
        std::cout << "faiss: index:" << fs_index_output[i] << "\t";
        std::cout << "distance:" << fs_distance_output[i];
        std::cout << std::endl;
    }

    bm_free_device(handle, sdc_table_input_dev);
    bm_free_device(handle, nxcodes_input_dev);
    bm_free_device(handle, nycodes_input_dev);
    bm_free_device(handle, distance_output_dev);
    bm_free_device(handle, index_output_dev);

    delete[] query_input_sys;
    delete[] database_input_sys;
    delete[] sdc_table_input_sys;
    delete[] nxcodes_input_sys;
    delete[] nycodes_input_sys;

    delete[] fs_query_codes;
    delete[] fs_distance_output;
    delete[] fs_index_output;
    delete[] distance_output_sys;
    delete[] index_output_sys;

    bm_dev_free(handle);
    return BM_SUCCESS;
}
#endif

bm_status_t faiss_indexPQ_ADC_test(
    int ny,
    int nx,
    int d,
    int m,
    int ks,
    int ds,
    int k,
    int seed,
    int IP_metric,
    int show_result)
{
    bm_handle_t handle;
    bm_status_t ret = BM_SUCCESS;
    ret = bm_dev_request(&handle, 0);
    if (BM_SUCCESS != ret)
    {
        std::cout << "request dev failed" << std::endl;
        return BM_ERR_FAILURE;
    }

    /// random input
    std::mt19937 rng;
    rng.seed(seed);
    std::uniform_real_distribution<float> dist_value(-10., 10.);
    std::uniform_int_distribution<int> dist_index(0, 255);

    std::cout << "------------set input-----------" << std::endl;
    float *centroids_input_sys = new float[m * ks * ds];
    float *nxquery_input_sys = new float[nx * d];
    unsigned char *nycodes_input_sys = new unsigned char[ny * m];
    float *distance_output_sys = new float[nx * ny];
    int *index_output_sys = new int[nx * ny];

    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < ks; j++)
        {
            for (int k = 0; k < ds; k++)
            {
                centroids_input_sys[i * ds * ks + j * ds + k] = dist_value(rng);
            }
        }
    }
    for (int i = 0; i < nx; i++)
    {
        for (int j = 0; j < d; j++)
        {
            nxquery_input_sys[i * d + j] = dist_value(rng);
        }
    }
    for (int i = 0; i < ny; i++)
    {
        for (int j = 0; j < m; j++)
        {
            nycodes_input_sys[i * m + j] = dist_index(rng);
        }
    }

    std::cout << "------------tpu input-----------" << std::endl;
    bm_device_mem_t centroids_input_dev;
    bm_device_mem_t nxquery_input_dev;
    bm_device_mem_t nycodes_input_dev;
    bm_device_mem_t distance_output_dev;
    bm_device_mem_t index_output_dev;

    int centroids_size = m * ks * ds * sizeof(float);
    int nxquery_size = nx * d * sizeof(float);
    int nycodes_size = ny * m * sizeof(char);
    int distance_size = nx * ny * sizeof(float);
    int index_size = nx * ny * sizeof(int);

    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &centroids_input_dev, centroids_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &nxquery_input_dev, nxquery_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &nycodes_input_dev, nycodes_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &distance_output_dev, distance_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &index_output_dev, index_size));

    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, centroids_input_dev, centroids_input_sys));
    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, nxquery_input_dev, nxquery_input_sys));
    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, nycodes_input_dev, nycodes_input_sys));

    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_faiss_indexPQ_ADC(handle,
                           centroids_input_dev,
                           nxquery_input_dev,
                           nycodes_input_dev,
                           distance_output_dev,
                           index_output_dev,
                           d, m, ks, ny, nx, k, IP_metric);
    gettimeofday(&t2, NULL);
    std::cout << "TPU using time(us): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)" << std::endl;
    std::cout << "TPU using time(ms): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / 1000 << "(ms)" << std::endl;

    if(ret != BM_SUCCESS){
        bm_free_device(handle, centroids_input_dev);
        bm_free_device(handle, nxquery_input_dev);
        bm_free_device(handle, nycodes_input_dev);
        bm_free_device(handle, distance_output_dev);
        bm_free_device(handle, index_output_dev);

        delete[] centroids_input_sys;
        delete[] nxquery_input_sys;
        delete[] nycodes_input_sys;
        delete[] distance_output_sys;
        delete[] index_output_sys;

        bm_dev_free(handle);
        return BM_ERR_FAILURE;
    }

    BMLIB_SAFE_CALL(bm_memcpy_d2s(handle, distance_output_sys, distance_output_dev));
    BMLIB_SAFE_CALL(bm_memcpy_d2s(handle, index_output_sys, index_output_dev));

    if (show_result)
    {
        std::cout << "ADCsearch result:" << std::endl;
        for (int i = 0; i < k; i++)
        {
            std::cout << "top:" << i + 1 << std::endl;
            std::cout << "index:" << index_output_sys[i] << "\t";
            std::cout << "distance:" << distance_output_sys[i];
            std::cout << std::endl;
        }
    }

    bm_free_device(handle, centroids_input_dev);
    bm_free_device(handle, nxquery_input_dev);
    bm_free_device(handle, nycodes_input_dev);
    bm_free_device(handle, distance_output_dev);
    bm_free_device(handle, index_output_dev);

    delete[] centroids_input_sys;
    delete[] nxquery_input_sys;
    delete[] nycodes_input_sys;
    delete[] distance_output_sys;
    delete[] index_output_sys;

    bm_dev_free(handle);
    return ret;
}

bm_status_t faiss_indexPQ_SDC_test(
    int ny,
    int nx,
    int m,
    int ks,
    int k,
    int seed,
    int IP_metric,
    int show_result)
{
    bm_handle_t handle;
    bm_status_t ret = BM_SUCCESS;
    ret = bm_dev_request(&handle, 0);
    if (BM_SUCCESS != ret)
    {
        std::cout << "request dev failed" << std::endl;
        return BM_ERR_FAILURE;
    }

    /// random input
    std::mt19937 rng;
    rng.seed(seed);
    std::uniform_real_distribution<float> dist_value(0, 20);
    std::uniform_int_distribution<int> dist_index(0, 255);

    float *sdc_table_input_sys = new float[m * ks * ks];
    unsigned char *nxcodes_input_sys = new unsigned char[nx * m];
    unsigned char *nycodes_input_sys = new unsigned char[ny * m];
    float *distance_output_sys = new float[nx * ny];
    int *index_output_sys = new int[nx * ny];

    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < ks; j++)
        {
            for (int k = 0; k < ks; k++)
            {
                if (k > j)
                {
                    sdc_table_input_sys[i * ks * ks + j * ks + k] = dist_value(rng);
                }
                else if (k < j)
                {
                    sdc_table_input_sys[i * ks * ks + j * ks + k] =  sdc_table_input_sys[i * ks * ks + k * ks + j];
                }
                else
                {
                    sdc_table_input_sys[i * ks * ks + j * ks + k] =  0;
                }
            }
        }
    }
    for (int i = 0; i < nx; i++)
    {
        for (int j = 0; j < m; j++)
        {
            nxcodes_input_sys[i * m + j] = dist_index(rng);
        }
    }
    for (int i = 0; i < ny; i++)
    {
        for (int j = 0; j < m; j++)
        {
            nycodes_input_sys[i * m + j] = dist_index(rng);
        }
    }

    int sdc_table_size = m * ks * ks * sizeof(float);
    int nxcodes_size = nx * m * sizeof(char);
    int nycodes_size = ny * m * sizeof(char);
    int output_distance_size = nx * ny * sizeof(float);
    int output_index_size = nx * ny * sizeof(int);

    bm_device_mem_t sdc_table_input_dev;
    bm_device_mem_t nxcodes_input_dev;
    bm_device_mem_t nycodes_input_dev;
    bm_device_mem_t distance_output_dev;
    bm_device_mem_t index_output_dev;

    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &sdc_table_input_dev, sdc_table_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &nxcodes_input_dev, nxcodes_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &nycodes_input_dev, nycodes_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &distance_output_dev, output_distance_size));
    BMLIB_SAFE_CALL(bm_malloc_device_byte(handle, &index_output_dev, output_index_size));

    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, sdc_table_input_dev, sdc_table_input_sys));
    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, nxcodes_input_dev, nxcodes_input_sys));
    BMLIB_SAFE_CALL(bm_memcpy_s2d(handle, nycodes_input_dev, nycodes_input_sys));

    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_faiss_indexPQ_SDC(handle,
                           sdc_table_input_dev,
                           nxcodes_input_dev,
                           nycodes_input_dev,
                           distance_output_dev,
                           index_output_dev,
                           m, ks, ny, nx, k, IP_metric);
    gettimeofday(&t2, NULL);
    std::cout << "TPU using time(us): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)" << std::endl;
    std::cout << "TPU using time(ms): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / 1000 << "(ms)" << std::endl;

    if(ret != BM_SUCCESS){
        bm_free_device(handle, sdc_table_input_dev);
        bm_free_device(handle, nxcodes_input_dev);
        bm_free_device(handle, nycodes_input_dev);
        bm_free_device(handle, distance_output_dev);
        bm_free_device(handle, index_output_dev);

        delete[] sdc_table_input_sys;
        delete[] nxcodes_input_sys;
        delete[] nycodes_input_sys;
        delete[] distance_output_sys;
        delete[] index_output_sys;

        bm_dev_free(handle);
        return BM_ERR_FAILURE;
    }

    BMLIB_SAFE_CALL(bm_memcpy_d2s(handle, distance_output_sys, distance_output_dev));
    BMLIB_SAFE_CALL(bm_memcpy_d2s(handle, index_output_sys, index_output_dev));

    if (show_result)
    {
        std::cout << "SDCsearch result:" << std::endl;
        for (int i = 0; i < k; i++)
        {
            std::cout << "top:" << i + 1 << std::endl;
            std::cout << "index:" << index_output_sys[i] << "\t";
            std::cout << "distance:" << distance_output_sys[i];
            std::cout << std::endl;
        }
    }

    bm_free_device(handle, sdc_table_input_dev);
    bm_free_device(handle, nxcodes_input_dev);
    bm_free_device(handle, nycodes_input_dev);
    bm_free_device(handle, distance_output_dev);
    bm_free_device(handle, index_output_dev);

    delete[] sdc_table_input_sys;
    delete[] nxcodes_input_sys;
    delete[] nycodes_input_sys;
    delete[] distance_output_sys;
    delete[] index_output_sys;

    bm_dev_free(handle);
    return ret;
}

int main(int argc, char *args[])
{
    unsigned int seed = (unsigned)time(NULL);
    srand(seed);

    /// set param
    int ny = 50000;
    int nx = 10;
    int k = 100;
    int d = 256;
    int m = 32;
    int is_sdc = 0;
    int IP_metric = 0;
    int show_result = 0;

    int nbits = 8;
    int ksub = 1 << nbits;

    if (argc > 1) ny = atoi(args[1]);
    if (argc > 2) nx = atoi(args[2]);
    if (argc > 3) k = atoi(args[3]);
    if (argc > 4) d = atoi(args[4]);
    if (argc > 5) m = atoi(args[5]);
    if (argc > 6) is_sdc = atoi(args[6]);
    if (argc > 7) IP_metric = atoi(args[7]);
    if (argc > 8) show_result = atoi(args[8]);

    std::cout << "------------parameter------------" << std::endl;
    std::cout << "database_num: " << ny << std::endl;
    std::cout << "query_num:    " << nx << std::endl;
    std::cout << "sort_count:   " << k << std::endl;
    std::cout << "data_dims:    " << d << std::endl;
    std::cout << "slice_num:    " << m << std::endl;
    std::cout << "sdc_type:     " << is_sdc << std::endl;
    std::cout << "ip_metric:    " << IP_metric << std::endl;
    std::cout << "show_result:  " << show_result << std::endl;
    std::cout << "random_seed:  " << seed << std::endl;

#if TEST_WITH_FAISS
    if (is_sdc)
    {
        if (BM_SUCCESS != compare_result_with_faiss_SDC(ny, nx, d, m, ksub, k, seed, IP_metric))
        {
            return -1;
        }
    }
    else
    {
        if (BM_SUCCESS != compare_result_with_faiss_ADC(ny, nx, d, m, ksub, dsub, k, seed, IP_metric))
        {
            return -1;
        }
    }

    return 0;
#endif

    if (is_sdc)
    {
        if (BM_SUCCESS != faiss_indexPQ_SDC_test(ny, nx, m, ksub, k, seed, IP_metric, show_result))
        {
            printf("test faiss_indexPQ_SDC failed! \n");
            return -1;
        }else{
            printf("test faiss_indexPQ_SDC successful! \n");
        }
    }
    else
    {
        if (BM_SUCCESS != faiss_indexPQ_ADC_test(ny, nx, d, m, ksub, d/m, k, seed, IP_metric, show_result))
        {
            printf("test faiss_indexPQ_ADC failed \n");
            return -1;
        }else{
            printf("test faiss_indexPQ_ADC successful! \n");
        }
    }

    return 0;
}