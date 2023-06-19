#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <assert.h>
#include <math.h>
#include <cmath>
#include "bmcv_api_ext.h"
#include "test_misc.h"

#ifdef __linux__
  #include <sys/time.h>
#else
  #include <windows.h>
#endif
#include <typeinfo>

using namespace std;

#define  SAVE_IMG   0
#define  BMCV_DESCALE(x,n)     (((x) + (1 << ((n)-1))) >> (n))

char* img_path = NULL;
char* opencvFile_path = NULL;

template <class T>
class BmcvPoint {
public:
    T x;
    T y;
    BmcvPoint() {}
    BmcvPoint(T ix, T iy): x(ix), y(iy) {}
    BmcvPoint operator= (const BmcvPoint& pt) {
        this->x = pt.x;
        this->y = pt.y;
        return *this;
    }
};

typedef BmcvPoint<float> BmcvPoint2f;
typedef BmcvPoint<int> BmcvPoint2i;

static int fill_v2(
        unsigned char* prev_data,
        unsigned char* next_data,
        int& ptsNum,
        bmcv_point2f_t* prev_pts,
        int width,
        int height){
    ifstream preImgread((string(opencvFile_path) + string("/lkpyramid_preImg.bin")), ios::in | ios::binary);
    ifstream nextImgread((string(opencvFile_path) + string("/lkpyramid_nextImg.bin")), ios::in | ios::binary);
    if(!preImgread || !nextImgread){
        cout << "Error opening file" << endl;
        return -1;
    }
    preImgread.read((char*)prev_data, sizeof(unsigned char) * height * width);
    nextImgread.read((char*)next_data, sizeof(unsigned char) * height * width);
    ifstream preImgPoints_readfile((string(opencvFile_path) + string("/preImg_points.bin")), ios::in | ios::binary);
    if(!preImgPoints_readfile){
        cout << "Error opening file" << endl;
        return -1;
    }
    for(int i = 0; i < ptsNum;i++){
        preImgPoints_readfile.seekg(sizeof(prev_pts[i].x) * 2 * i, ios::beg);
        int r = preImgPoints_readfile.tellg();
        preImgPoints_readfile.read((char*)(&prev_pts[i].x),sizeof(prev_pts[i].x));
        preImgPoints_readfile.seekg((r + 4), ios::beg);
        preImgPoints_readfile.read((char*)(&prev_pts[i].y), sizeof(prev_pts[i].y));
    }
    preImgread.close();
    nextImgread.close();
    preImgPoints_readfile.close();
    return 0;
}

static void bmcv_calc_sharr_deriv(
        int cn,
        int width,
        int height,
        int dstStep,
        int kh,
        unsigned char* src,
        short* dst) {
    int widthn = width * cn;
    int x, y, delta = (width + 2) * cn;
    short* _tempBuf = new short [delta * 2];
    short* trow0 = _tempBuf + cn, *trow1 = trow0 + delta;

    for (y = 0; y < height; y++) {
        const unsigned char* srow0 = src + (y > 0 ? y-1 : height > 1 ? 1 : 0) * width;
        const unsigned char* srow1 = src + y * width;
        const unsigned char* srow2 = src + (y < height - 1 ? y + 1 : height > 1 ? height-2 : 0) * width;
        short* xdrow = dst + y * dstStep;
        short* ydrow = dst + y * dstStep + (height + 2 * kh) * dstStep;
        // do vertical convolution
        x = 0;
        for (; x < widthn; x++) {
            int t0 = (srow0[x] + srow2[x]) * 3 + srow1[x] * 10;
            int t1 = srow2[x] - srow0[x];
            trow0[x] = (short)t0;
            trow1[x] = (short)t1;
        }
        // make border
        int x0 = (width > 1 ? 1 : 0) * cn, x1 = (width > 1 ? width - 2 : 0) * cn;
        for (int k = 0; k < cn; k++) {
            trow0[-cn + k] = trow0[x0 + k]; trow0[widthn + k] = trow0[x1 + k];
            trow1[-cn + k] = trow1[x0 + k]; trow1[widthn + k] = trow1[x1 + k];
        }
        // do horizontal convolution, interleave the results and store them to dst
        x = 0;
        for (; x < widthn; x++) {
            short t0 = (short)(trow0[x+cn] - trow0[x-cn]);
            short t1 = (short)((trow1[x+cn] + trow1[x-cn])*3 + trow1[x]*10);
            xdrow[x] = t0; ydrow[x] = t1;
        }
    }
    delete [] _tempBuf;
}

static int lkpyramid_tpu(
        unsigned char* prevPtr,
        unsigned char* nextPtr,
        bmcv_point2f_t* prevPts,
        bmcv_point2f_t* nextPts,
        bool* status,
        int ptsNum,
        int width,
        int height,
        int kw,
        int kh,
        int maxLevel,
        bmcv_term_criteria_t criteria) {
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }
    ret = bmcv_open_cpu_process(handle);
    if (ret != BM_SUCCESS) {
        printf("BMCV enable CPU failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }
    bm_image_format_ext fmt = FORMAT_GRAY;
    bm_image prevImg;
    bm_image nextImg;
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &prevImg);
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &nextImg);
    bm_image_alloc_dev_mem(prevImg);
    bm_image_alloc_dev_mem(nextImg);
    bm_image_copy_host_to_device(prevImg, (void **)(&prevPtr));
    bm_image_copy_host_to_device(nextImg, (void **)(&nextPtr));
    void *plan = nullptr;
    bmcv_image_lkpyramid_create_plan(
            handle,
            plan,
            width,
            height,
            kw,
            kh,
            maxLevel);
    struct timeval t1, t2;

    gettimeofday_(&t1);

    bmcv_image_lkpyramid_execute(
            handle,
            plan,
            prevImg,
            nextImg,
            ptsNum,
            prevPts,
            nextPts,
            status,
            criteria);
    gettimeofday_(&t2);

    cout << "lkpyramid_execute TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;
    bmcv_image_lkpyramid_destroy_plan(handle, plan);
    bm_image_destroy(prevImg);
    bm_image_destroy(nextImg);
    ret = bmcv_close_cpu_process(handle);
    if (ret != BM_SUCCESS) {
        printf("BMCV disable CPU failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }
    bm_dev_free(handle);
    return 0;
}

static int cmp(
        bool* status_got,
        bool* status_exp,
        bmcv_point2f_t* pts_got,
        bmcv_point2f_t* pts_exp,
        int ptsNum,
        float delta
        ) {
    for (int i = 0; i < ptsNum; i++) {
        if (status_exp[i] != status_got[i]) {
            printf("cmp error: %d status is diff\n", i);
            return -1;
        }
        if (!status_got[i]) continue;
        if (abs(pts_got[i].x - pts_exp[i].x) > delta || abs(pts_got[i].y - pts_exp[i].y) > delta) {
            printf("cmp error: %d coordinate is diff.\n", i);
            printf("exp: [%f, %f]\ngot: [%f, %f]\n", pts_exp[i].x, pts_exp[i].y, pts_got[i].x, pts_got[i].y);
            return -1;
        }
    }
    return 0;
}

static int test_lkpyramid_random(
        int height,
        int width
        ) {
    // int winH = 21;
    // int winW = 21;
    int winH = 41;
    int winW = 47;
    int maxLevel = 3;
    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed = tp.tv_nsec;
    srand(seed);
    cout << "seed = " << seed << endl;
    // maxLevel = rand() % 4;
    // int tmp = (100 + rand() % 1820) / pow(2, (1+ maxLevel));
    int tmp = (100 + 1050) / pow(2, (1+ maxLevel));
    width = tmp * pow(2, (1+ maxLevel));
    height = 1056;
    int channel = 1;
    int ptsNum = 10;
    bmcv_term_criteria_t criteria = {3, 10, 0.03};
    unsigned char* prev_data = new unsigned char [width * height * channel];
    unsigned char* next_data = new unsigned char [width * height * channel];
    bmcv_point2f_t* prev_pts = new bmcv_point2f_t [ptsNum];
    bmcv_point2f_t* next_pts_ocv = new bmcv_point2f_t [ptsNum];
    bmcv_point2f_t* next_pts_tpu = new bmcv_point2f_t [ptsNum];
    bool* status_ocv = new bool [ptsNum];
    bool* status_tpu = new bool [ptsNum];
    fill_v2(prev_data, next_data, ptsNum, prev_pts, width, height);
    cout << "width: " << width << "  height: " << height << endl;
    cout << "winW: " << winW << "  winH: " << winH << endl;
    cout << "maxLevel: " << maxLevel << "   ptsNum: " << ptsNum << endl;
    cout << "LK cpu averge using time: 8948um" << endl;
    ifstream opencv_readfile((string(opencvFile_path) + string("/lkpyramid_nextOcv.bin")), ios::in | ios::binary);
    if(!opencv_readfile){
        cout << "Error opening file" << endl;
        return -1;
    }
    for(int i = 0; i < ptsNum; i++){
        opencv_readfile.seekg(sizeof(next_pts_ocv[i].x) * 2 * i, ios::beg);
        int r = opencv_readfile.tellg();
        opencv_readfile.read((char*)(&next_pts_ocv[i].x),sizeof(next_pts_ocv[i].x));
        opencv_readfile.seekg((r + 4), ios::beg);
        opencv_readfile.read((char*)(&next_pts_ocv[i].y), sizeof(next_pts_ocv[i].y));
    }
    opencv_readfile.read((char*)status_ocv, sizeof(bool) * ptsNum);
    opencv_readfile.close();
    lkpyramid_tpu(
            prev_data,
            next_data,
            prev_pts,
            next_pts_tpu,
            status_tpu,
            ptsNum,
            width,
            height,
            winW,
            winH,
            maxLevel,
            criteria);
    //计算有带来较大的像素误差
    int ret = cmp(
            status_tpu,
            status_ocv,
            next_pts_tpu,
            next_pts_ocv,
            ptsNum,
            20);
    delete [] prev_data;
    delete [] next_data;
    delete [] prev_pts;
    delete [] next_pts_ocv;
    delete [] next_pts_tpu;
    delete [] status_ocv;
    delete [] status_tpu;
    return ret;
}

int main(int argc, char* args[]) {
    int loop = 1;
    int height = 1080;
    int width = 1920;
    if (argc > 1) loop = atoi(args[1]);
    opencvFile_path = getenv("BMCV_TEST_FILE_PATH");
    if (opencvFile_path == NULL){
        printf("please set environment vairable: BMCV_TEST_FILE_PATH !\n");
        return -1;
    }
    int ret = 0;
    for (int i = 0; i < loop; i++) {
        ret = test_lkpyramid_random(height, width);
        if (ret) {
            cout << "test LK-pyramid failed" << endl;
            return ret;
        }
    }
    cout << "Compare TPU result with CPU successfully!" << endl;
    return 0;
}

// test performance:
// 1080P  GRAY
// ptsNum=10 winSize=21 maxLevel=3
// criteria = {3, 10, 0.03}
// time (pcie enable cpu):  5431um

