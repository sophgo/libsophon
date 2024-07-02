/*****************************************************************************
 *
 *    Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
 *
 *    bmvid is licensed under the 2-Clause BSD License except for the
 *    third-party components.
 *
 *****************************************************************************/
/* This library provides a high-level interface for controlling the BitMain
 * Sophon VPU en/decoder.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef __linux__
#include <pthread.h>
#include <sys/time.h>
#include <getopt.h>
#elif _WIN32
#include "windows/libusb-1.0.18/examples/getopt/getopt.h" //use chip media windows src directly
#include <windows.h>
#include <time.h>
#endif
#include <signal.h>

#include "bmvpuenc.h"
#include "bmqueue.h"
#define INTERVAL 1
#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct {
    int soc_idx; /* only for pcie mode */

    int enc_fmt; /* 0: H.264, 1: H.265 */

    int pix_fmt; /* 0: yuv420, 1: nv12 */

    int crop_w;  /* actual width  */
    int crop_h;  /* actual height */

    int y_stride;
    int c_stride;

    int aligned_height;

    int bit_rate; /* kbps */
    int fps;  /*default is 30*/
    int cqp;

    int gop_preset;
    int intra_period;
} EncParameter;

typedef struct {
    char *input_filename;
    FILE *fin;
    char *output_filename;

    int   log_level;
    int   thread_number; /* at most 24 threads for now */
    int   thread_id;

    int   loop;
    int   frame_number;
    EncParameter enc;

    int   result;

    int   run_times;
} InputParameter;

typedef struct {
    BmVpuEncOpenParams open_params;
    BmVpuEncoder* video_encoder;

    BmVpuEncInitialInfo initial_info;

    BmVpuFramebuffer* src_fb_list;
    BmVpuEncDMABuffer* src_fb_dmabuffers;
    void*             frame_unused_queue;
    int num_src_fb;
    BmVpuFramebuffer* src_fb;

    BmVpuEncDMABuffer bs_dma_buffer;
    size_t bs_buffer_size;
    uint32_t bs_buffer_alignment;

    BmVpuEncParams enc_params;
    BmVpuRawFrame input_frame;
    BmVpuEncodedFrame output_frame;

    FILE*  fin;
    FILE*  fout;
    int    soc_idx;  /* only for pcie mode */
    int    core_idx; /* the index of video encoder core */

    int    preset;   /* 0, 1, 2 */

    struct timeval ps;
    struct timeval pe;
    double total_time;
    long long   total_frame;
} VpuEncContext;

typedef struct {
    int        number;
#ifdef __linux__
    pthread_t* handles;
#elif _WIN32
    HANDLE *handles;
#endif
} threads_t;
#ifdef __linux__
static sigset_t waitsigset = {0};
#endif
#ifdef BM_PCIE_MODE
#define MAX_THREAD 24
#else
#define MAX_THREAD 18
#endif
long long count_enc[MAX_THREAD];
double sta_time[MAX_THREAD];
int g_exit_flag = 0;
#ifdef __linux__
    pthread_t thread_stat;
#elif _WIN32
    HANDLE thread_stat;
#endif

static void usage(char *progname);
static int parse_args(int argc, char **argv, InputParameter* par);
static int read_yuv_source(uint8_t* dst_pa, int dst_stride_y, int dst_stride_c, int dst_height,
                           FILE**  src_file, int src_stride_y, int src_stride_c, int src_height,
                           int pix_format,
                           int width, int height);

static BmVpuFramebuffer* get_src_framebuffer(VpuEncContext *ctx);
#ifdef _WIN32
static int s_gettimeofday(struct timeval* tp, void* tzp)
{
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    tm.tm_year = wtm.wYear - 1900;
    tm.tm_mon = wtm.wMonth - 1;
    tm.tm_mday = wtm.wDay;
    tm.tm_hour = wtm.wHour;
    tm.tm_min = wtm.wMinute;
    tm.tm_sec = wtm.wSecond;
    tm.tm_isdst = -1;
    clock = mktime(&tm);
    tp->tv_sec = clock;
    tp->tv_usec = wtm.wMilliseconds * 1000;
    return (0);
}
int win_exit_flag = 0;
static BOOL CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {

    case CTRL_C_EVENT:
        win_exit_flag = 1;
        return(TRUE);
    default:
        return FALSE;
    }
}
#endif
#if _WIN32
DWORD statPthread(void *arg)
{
#else
void *stat_pthread(void *arg)
{
#endif
    InputParameter* ctx = (InputParameter*)arg;
    int thread_num = ctx->thread_number;
    int i = 0;
    long long   last_count_enc[MAX_THREAD]      = {0};
    double      last_time[MAX_THREAD]           = {0.0};
    double      cur_fps[MAX_THREAD]             = {0.0};
    double      total_fps                       = 0.0;
    long long   count_sum                       = 0;
    int         dis_mode = 0;
    char        *display = getenv("BMVPUENC_DISPLAY_FRAMERATE");
    if (display) dis_mode = atoi(display);
    printf("BMVPUENC_DISPLAY_FRAMERATE=%d  thread_num=%d g_exit_flag=%d \n", dis_mode, thread_num, g_exit_flag);
    while(!g_exit_flag) {
#ifdef __linux__
        sleep(INTERVAL);
#elif _WIN32
        Sleep(INTERVAL*1000);
#endif
        total_fps = 0.0;
        for (i = 0; i < thread_num; i++){
            count_sum += count_enc[i] - last_count_enc[i];
            if ((count_enc[i] > last_count_enc[i]) && (sta_time[i] > last_time[i])) {
                cur_fps[i] = (double)((count_enc[i]-last_count_enc[i]) * 1000)/(sta_time[i]-last_time[i]);
            } else {
                cur_fps[i] = 0;
            }

            total_fps += cur_fps[i];
            last_count_enc[i] = count_enc[i];
            last_time[i] = sta_time[i];
        }
        if (dis_mode == 1) {
            for (i = 0; i < thread_num; i++)
                printf("ID[%d],   FRM[%10lld], Cost Time[%2.2f] FPS[%2.2f]  \n", i, (long long)count_enc[i], sta_time[i], cur_fps[i]);
        }else
            printf("thread_count %d, total_frame %lld, total_enc_fps %2.2f\n", thread_num, count_sum, total_fps);
        printf("\r");
        fflush(stdout);
    }
    fflush(stdout);

    for (i = 0; i < thread_num; i++)
        printf("%3dth thread Encode %lld frame in total, avg: %5.4f, time: %5.4fms!\n", i, count_enc[i], (double)count_enc[i] * 1000 / sta_time[i], sta_time[i]);
    printf("stat_pthread over.\n");
    return NULL;
}

static void* acquire_output_buffer(void *context, size_t size, void **acquired_handle)
{
    ((void)(context));
    void *mem;

    mem = malloc(size);
    *acquired_handle = mem;
    return mem;
}
static void finish_output_buffer(void *context, void *acquired_handle)
{
    ((void)(context));
}

static void cleanup_task(void* arg)
{
    VpuEncContext* ctx = (void*)arg;
    int i;
#ifdef __linux__
    printf("Clean up thread 0x%lx\n", pthread_self());
#elif _WIN32
    printf("Clean up thread 0x%lx\n", GetCurrentThreadId());
#endif
    if (ctx==NULL)
        return;

    /* Close the previously opened encoder instance */
    if (ctx->video_encoder)
    {
        bmvpu_enc_close(ctx->video_encoder);
    }

    if (ctx->frame_unused_queue != NULL)
    {
        bm_queue_destroy(ctx->frame_unused_queue);
        ctx->frame_unused_queue = NULL;
    }

    /* Free all allocated memory (both regular and DMA memory) */
    if (ctx->src_fb_list)
        free(ctx->src_fb_list);
    if (ctx->src_fb_dmabuffers)
    {
        for (i = 0; i < ctx->num_src_fb; ++i)
            bmvpu_enc_dma_buffer_deallocate(ctx->core_idx, &(ctx->src_fb_dmabuffers[i]));

        free(ctx->src_fb_dmabuffers);
    }

    if (&(ctx->bs_dma_buffer))
        bmvpu_enc_dma_buffer_deallocate(ctx->core_idx, &(ctx->bs_dma_buffer));

    /* Unload the VPU firmware */
    bmvpu_enc_unload(ctx->soc_idx);

    if (ctx->fout != NULL)
        fclose(ctx->fout);

    if (ctx != NULL)
        free(ctx);
}

static int run_once(InputParameter* par)
{
    VpuEncContext* ctx = NULL;
    BmVpuEncOpenParams* eop = NULL;
    BmVpuCodecFormat codec_fmt = BM_VPU_CODEC_FORMAT_H265;

    EncParameter* enc_par = &(par->enc);
    char output_filename[256] = {0};
    FILE *fin, *fout;
    int l, i, ret = 0;
    int tid = par->thread_id;

    fin = par->fin;

    ret = strncmp(par->output_filename, "/dev/null", 9);
    if (ret != 0)
    {
        if (enc_par->enc_fmt == 0)
            sprintf(output_filename, "%s-%d.264", par->output_filename, tid);
        else
            sprintf(output_filename, "%s-%d.265", par->output_filename, tid);
        fout = fopen(output_filename, "wb");
    }
    else
    {
        fout = fopen("/dev/null", "wb");
    }

    if (fout == NULL)
    {
        fprintf(stderr, "Failed to open %s for writing: %s\n",
                output_filename, strerror(errno));
        fclose(fin);
        par->result = -1;
        return -1;
    }

    bmvpu_enc_set_logging_threshold(par->log_level);
    bmvpu_enc_set_logging_function(logging_fn);

    ctx = calloc(1, sizeof(VpuEncContext));
    if (ctx == NULL)
    {
        fprintf(stderr, "calloc failed\n");
        par->result = -1;
        return -1;
    }
#ifdef __linux__
    pthread_cleanup_push(cleanup_task, (void*)ctx);
    int old_canclestate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_canclestate);
#endif
    ctx->fin  = fin;
    ctx->fout = fout;

    ctx->soc_idx = enc_par->soc_idx;
    ctx->core_idx = bmvpu_enc_get_core_idx(enc_par->soc_idx);

    eop = &(ctx->open_params);

    if (enc_par->enc_fmt == 0)
        codec_fmt = BM_VPU_CODEC_FORMAT_H264;
    else
        codec_fmt = BM_VPU_CODEC_FORMAT_H265;

    bmvpu_enc_set_default_open_params(eop, codec_fmt);

    eop->soc_idx = ctx->soc_idx;

    /* If this is 1, then Cb and Cr are interleaved in one shared chroma
     * plane, otherwise they are separated in their own planes.
     * See the BmVpuEncPixFormat documentation for the consequences of this. */
    if (enc_par->pix_fmt == 0)
        eop->pix_format = BM_VPU_ENC_PIX_FORMAT_YUV420P;
    else
        eop->pix_format = BM_VPU_ENC_PIX_FORMAT_NV12;

    eop->frame_width  = enc_par->crop_w;
    eop->frame_height = enc_par->crop_h;
    eop->timebase_num = 1;
    eop->timebase_den = 1;
    eop->fps_num = par->enc.fps;
    eop->fps_den = 1;

    /* Set a bitrate of 0 bps, which tells the VPU to use constant quality mode. */
    eop->bitrate = enc_par->bit_rate*1000;

    eop->cqp = enc_par->cqp;

    eop->intra_period = enc_par->intra_period;
    eop->gop_preset = enc_par->gop_preset;

    eop->buffer_alloc_func = NULL;
    eop->buffer_free_func = NULL;
    eop->buffer_context = NULL;

    /* Load the VPU firmware */
    ret = bmvpu_enc_load(ctx->soc_idx);
    if (ret != BM_VPU_ENC_RETURN_CODE_OK)
    {
        fprintf(stderr, "bmvpu_enc_load failed!\n");
        ret = -1;
        goto cleanup;
    }

    /* Retrieve information about the required bitstream buffer */
    bmvpu_enc_get_bitstream_buffer_info(&(ctx->bs_buffer_size), &(ctx->bs_buffer_alignment));

    /* in unit of 4k bytes */
    ctx->bs_buffer_size = (ctx->bs_buffer_size +(4*1024-1)) & (~(4*1024-1));

    /* Create bs buffer */
    ret = bmvpu_enc_dma_buffer_allocate(ctx->core_idx, &(ctx->bs_dma_buffer), ctx->bs_buffer_size);
    if (ret != 0)
    {
        fprintf(stderr, "bm_malloc_device_byte for bs_dmabuffer failed!\n");
        ret = -1;
        goto cleanup;
    }

    /* Open an encoder instance, using the previously allocated bitstream buffer */
    ret = bmvpu_enc_open(&(ctx->video_encoder), eop, &(ctx->bs_dma_buffer), &(ctx->initial_info));
    if (ret != BM_VPU_ENC_RETURN_CODE_OK)
    {
        fprintf(stderr, "bmvpu_enc_open failed\n");
        ret = -1;
        goto cleanup;
    }
    if (!ctx->video_encoder)
    {
        fprintf(stderr, "ctx->video_encoder is NULL!\n");
        ret = -1;
        goto cleanup;
    }

    ctx->num_src_fb = ctx->initial_info.min_num_src_fb;
    printf("num framebuffers for src: %u\n", ctx->num_src_fb);

    ctx->src_fb_list = (BmVpuFramebuffer *)malloc(sizeof(BmVpuFramebuffer) * ctx->num_src_fb);
    if (ctx->src_fb_list == NULL)
    {
        fprintf(stderr, "malloc failed\n");
        ret = -1;
        goto cleanup;
    }
    ctx->src_fb_dmabuffers = (BmVpuEncDMABuffer*)malloc(sizeof(BmVpuEncDMABuffer) * ctx->num_src_fb);
    if (ctx->src_fb_dmabuffers == NULL)
    {
        fprintf(stderr, "malloc failed\n");
        ret = -1;
        goto cleanup;
    }
    for (i = 0; i < ctx->num_src_fb; ++i)
    {
        int src_id = 0x100 + (tid<<5) + i;

        /* Allocate DMA buffers for the raw input frames. */
        ret = bmvpu_enc_dma_buffer_allocate(ctx->core_idx, &ctx->src_fb_dmabuffers[i], ctx->initial_info.src_fb.size);
        if(ret != 0){
            fprintf(stderr, "bmvpu_malloc_device_byte_heap for src_buffer failed\n");
            ret = -1;
            goto cleanup;
        }

        ret = bmvpu_fill_framebuffer_params(&(ctx->src_fb_list[i]),
                                      &(ctx->initial_info.src_fb),
                                      &(ctx->src_fb_dmabuffers[i]),
                                      src_id, NULL);
        if(ret != 0){
            fprintf(stderr, "bmvpu_fill_framebuffer_params failed\n");
            ret = -1;
            goto cleanup;
        }
    }

    /* Create queue for source frame unused */
    ctx->frame_unused_queue = bm_queue_create(ctx->num_src_fb, sizeof(BmVpuFramebuffer*));
    if (ctx->frame_unused_queue == NULL)
    {
        fprintf(stderr, "bm_queue_create failed\n");
        ret = -1;
        goto cleanup;
    }
    for (i=0; i<ctx->num_src_fb; i++)
    {
        BmVpuFramebuffer *fb = &(ctx->src_fb_list[i]);
        bm_queue_push(ctx->frame_unused_queue, (void*)(&fb));
        if (par->log_level > BMVPU_ENC_LOG_LEVEL_INFO)
#ifdef __linux__
            printf("[%zx] myIndex = 0x%x, %p, push\n", pthread_self(), fb->myIndex, fb);
#endif
#ifdef _WIN32
            printf("[%zx] myIndex = 0x%x, %p, push\n", GetCurrentThreadId(), fb->myIndex, fb);
#endif

    }
    bm_queue_show(ctx->frame_unused_queue);

    /* Set the encoding parameters for this frame. */
    memset(&(ctx->enc_params), 0, sizeof(BmVpuEncParams));
    ctx->enc_params.acquire_output_buffer = acquire_output_buffer;
    ctx->enc_params.finish_output_buffer  = finish_output_buffer;
    ctx->enc_params.output_buffer_context = NULL;
    ctx->enc_params.skip_frame = 0; // TODO

    /* Set up the input frame.
     * The only field that needs to be set is the input framebuffer.
     * The encoder will read from it.
     * The rest can remain zero/NULL. */
    memset(&(ctx->input_frame), 0, sizeof(ctx->input_frame));

    /* Set up the output frame.
     * Simply setting all fields to zero/NULL is enough.
     * The encoder will fill in data. */
    memset(&(ctx->output_frame), 0, sizeof(ctx->output_frame));

    int frame_size = ctx->initial_info.src_fb.size;
    uint8_t* host_va = malloc(sizeof(uint8_t)*frame_size);
    if (host_va==NULL)
    {
        fprintf(stderr, "malloc failed\n");
        ret = -1;
        goto cleanup;
    }

#ifdef __linux__
    pthread_setcancelstate(old_canclestate, NULL);
    pthread_testcancel();
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_canclestate);
#endif
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif
    l = 0;
    while (1)
    {
        fseek(fin, 0, SEEK_SET);
        /* Read input I420/NV12 frames and encode them until the end of the input file is reached */
        for (i=0; i<par->frame_number; i++)
        {
            void *output_block;
            uint32_t output_code;
#ifdef _WIN32
            if (win_exit_flag)
                break;
#endif
            /* Stop encoding if EOF was reached */
            if (feof(fin))
                break;

            ctx->src_fb = get_src_framebuffer(ctx);
            if (ctx->src_fb == NULL)
            {
                fprintf(stderr, "get_src_framebuffer failed\n");
                ret = -1;
                goto cleanup;
            }

            /* Read uncompressed pixels into the input DMA buffer */
            ret = read_yuv_source(host_va, ctx->initial_info.src_fb.y_stride, ctx->initial_info.src_fb.c_stride, ctx->initial_info.src_fb.height,
                                &fin, enc_par->y_stride, enc_par->c_stride, enc_par->aligned_height,
                                eop->pix_format,
                                enc_par->crop_w, enc_par->crop_h);
            if (ret < 0)
            {
                int j;
                for (j=0; j<ctx->num_src_fb; j++)
                {
                    BmVpuFramebuffer* fb = &(ctx->src_fb_list[j]);
                    if (fb->myIndex == ctx->src_fb->myIndex)
                    {
                        bm_queue_push(ctx->frame_unused_queue, &fb);
                        if (par->log_level > BMVPU_ENC_LOG_LEVEL_INFO)
#ifdef __linux__
                        printf("[%zx] myIndex = 0x%x, push\n", pthread_self(), fb->myIndex);
#endif
#ifdef _WIN32
                        printf("[%zx] myIndex = 0x%x, push\n", GetCurrentThreadId(), fb->myIndex);
#endif
                        break;
                    }
                }
                break;
            }

            // TODO
            u64 vpu_pa = bmvpu_enc_dma_buffer_get_physical_address(ctx->src_fb->dma_buffer);
            ret = bmvpu_enc_upload_data(ctx->core_idx, host_va, frame_size,
                                    vpu_pa, frame_size, frame_size, 1);
            if (ret != 0)
            {
                fprintf(stderr, "%s:%d(%s) bmvpu_enc_upload_data failed\n", __FILE__, __LINE__, __func__);
                ret = -1;
                goto cleanup;
            }

            ctx->input_frame.framebuffer = ctx->src_fb;

            if (par->log_level > BMVPU_ENC_LOG_LEVEL_ERROR)
                printf("\n\n");

#ifdef __linux__
            gettimeofday(&(ctx->ps), NULL);
#elif _WIN32
            s_gettimeofday(&(ctx->ps), NULL);
#endif
            /* The actual encoding */
            ret = bmvpu_enc_encode(ctx->video_encoder, &(ctx->input_frame), &(ctx->output_frame), &(ctx->enc_params), &output_code);
            if (ret != BM_VPU_ENC_RETURN_CODE_OK && ret != BM_VPU_ENC_RETURN_CODE_END)
            {
                fprintf(stderr, "%s:%d(%s) bmvpu_enc_encode failed\n", __FILE__, __LINE__, __func__);
                ret = -1;
                goto cleanup;
            }

            if (ctx->output_frame.data_size > 0)
            {
                int j;
                /* Collect the input frames released */
                for (j=0; j<ctx->num_src_fb; j++)
                {
                    BmVpuFramebuffer* fb = &(ctx->src_fb_list[j]);
                    if (fb->myIndex == ctx->output_frame.src_idx)
                    {
                        bm_queue_push(ctx->frame_unused_queue, &fb);
                        if (par->log_level > BMVPU_ENC_LOG_LEVEL_INFO)
#ifdef __linux__
                        printf("[%zx] myIndex = 0x%x, push\n", pthread_self(), fb->myIndex);
#endif
#ifdef _WIN32
                        printf("[%zx] myIndex = 0x%x, push\n", GetCurrentThreadId(), fb->myIndex);
#endif
                        break;
                    }
                }
                if (j >= ctx->num_src_fb)
                {
                    fprintf(stderr, "unknown encSrcIdx %d\n", ctx->output_frame.src_idx);
                }

#ifdef __linux__
                gettimeofday(&(ctx->pe), NULL);
#elif _WIN32
                s_gettimeofday(&(ctx->pe), NULL);
#endif
                sta_time[tid] += ((ctx->pe.tv_sec*1000.0 + ctx->pe.tv_usec/1000.0) -
                                    (ctx->ps.tv_sec*1000.0 + ctx->ps.tv_usec/1000.0));
                count_enc[tid]++;

                /* Write out the encoded frame to the output file. */
                output_block = ctx->output_frame.acquired_handle;
                fwrite(output_block, 1, ctx->output_frame.data_size, fout);
                free(output_block);
            }
#ifdef __linux__
            pthread_setcancelstate(old_canclestate, NULL);
            pthread_testcancel();
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_canclestate);
#endif
        }
#ifdef _WIN32
        if (win_exit_flag)
            break;
#endif
        if (par->loop > 0 && ++l >= par->loop)
            break;
    }

    free(host_va);

    while(1)
    {
        void *output_block;
        uint32_t output_code;

        ctx->input_frame.framebuffer = NULL;
        ctx->input_frame.context = NULL;
        ctx->input_frame.pts = 0L;
        ctx->input_frame.dts = 0L;

        if (par->log_level > BMVPU_ENC_LOG_LEVEL_ERROR)
            printf("\n\n");

#ifdef __linux__
        gettimeofday(&(ctx->ps), NULL);
#endif
#ifdef _WIN32
        s_gettimeofday(&(ctx->ps), NULL);
#endif


        /* The actual encoding */
        ret = bmvpu_enc_encode(ctx->video_encoder, &(ctx->input_frame), &(ctx->output_frame), &(ctx->enc_params), &output_code);
        if (ret == BM_VPU_ENC_RETURN_CODE_END)
        {
#ifdef __linux__
            printf("[%zx] Encoding end\n", pthread_self());
#endif
#ifdef _WIN32
            printf("[%zx] Encoding end\n", GetCurrentThreadId());
#endif
            break;
        }

        if (ret != BM_VPU_ENC_RETURN_CODE_OK)
        {
            fprintf(stderr, "%s:%d(%s) Error at flushing\n", __FILE__, __LINE__, __func__);
            break;
        }

        if (ctx->output_frame.data_size <= 0)
        {
            printf("Invalid data size of output\n");
            break;
        }

#ifdef __linux__
        gettimeofday(&(ctx->pe), NULL);
#elif _WIN32
        s_gettimeofday(&(ctx->pe), NULL);
#endif
        sta_time[tid] += ((ctx->pe.tv_sec*1000.0 + ctx->pe.tv_usec/1000.0) -
                            (ctx->ps.tv_sec*1000.0 + ctx->ps.tv_usec/1000.0));
        count_enc[tid]++;
        /* Write out the encoded frame to the output file. */
        output_block = ctx->output_frame.acquired_handle;
        fwrite(output_block, 1, ctx->output_frame.data_size, fout);
        free(output_block);
    }

cleanup:
    cleanup_task((void*)ctx);
    par->result = ret;
#ifdef __linux__
    pthread_cleanup_pop(0);
#endif
    return ret;
}

static int run(InputParameter* par)
{
    int ret = 0;
    int i;
    par->fin = fopen(par->input_filename, "rb");
    if (par->fin == NULL)
    {
        fprintf(stderr, "Failed to open %s for reading: %s\n",
                par->input_filename, strerror(errno));
        par->result = -1;
        return -1;
    }
    for(i=0; i<par->run_times; i++)
    {
        fseek(par->fin, 0, SEEK_SET);
        ret = run_once(par);
        if(ret < 0)
            break;
    }
    fclose(par->fin);
    return ret;
}

static void usage(char *progname)
{
    static char options[] =
    "\t-v logging level\n"
    "\t   0: Error(default), 1: Warning, 2: Info, 3: Debug, 4: Log, 5: Trace\n"
    "\t-s    Sophon device index. (only for PCIE mode). (deprecated)\n"
    "\t--soc Sophon device index. (only for PCIE mode)\n"
    "\t\tFor example, if /dev/bm-sophon1 will be used, please set -s 1\n"
    "\t\tor set --soc 1\n"
    "\t-m thread number: 1(default), pcie max 24, soc max 18\n"
    "\t-f         pixel format. 0: YUV420(default); 1: NV12\n"
    "\t--pix_fmt  pixel format. 0: YUV420(default); 1: NV12\n"
    "\t-c         video encoder. 0: H.264; 1: H.265(default)\n"
    "\t--codec    video encoder. 0: H.264; 1: H.265(default)\n"
    "\t-p         intra period. Default value is 28. (deprecated)\n"
    "\t--intra    intra period. Default value is 28.\n"
    "\t-g            gop preset index. (deprecated)\n"
    "\t--gop_preset  gop preset index\n"
    "\t   1: all I,          all Intra, gopsize = 1\n"
    "\t   2: I-P-P,          consecutive P, cyclic gopsize = 1\n"
    "\t   3: I-B-B-B,        consecutive B, cyclic gopsize = 1\n"
    "\t   4: I-B-P-B-P,      gopsize = 2\n"
    "\t   5: I-B-B-B-P,      gopsize = 4 (default)\n"
    "\t   6: I-P-P-P-P,      consecutive P, cyclic gopsize = 4\n"
    "\t   7: I-B-B-B-B,      consecutive B, cyclic gopsize = 4\n"
    "\t   8: random access, I-B-B-B-B-B-B-B-B, cyclic gopsize = 8\n"
    "\t   Low delay cases are 1, 2, 3, 6, 7\n"
    "\t-w        actual width\n"
    "\t--width   actual width\n"
    "\t-h        actual height\n"
    "\t--height  actual height\n"
    "\t-y        luma stride (optional)\n"
    "\t--stride  luma stride (optional)\n"
    "\t-t aligned height (optional)\n"
    "\t-r bit rate (kbps). 0 means to use constant quality mode (0 at default)\n"
    "\t-q quantization parameter for constant quality mode. [0, 51] (32 at default)\n"
    "\t--run_times  open and close codec count. (1 at default)\n"
    "\t-l      loop number(optional). (1 at default, 0 is press test)\n"
    "\t--loop  loop number(optional). (1 at default, 0 is press test)\n"
    "\t-n output frame number(optional). -1,0: unlimited encoding\n"
    "\t-a       framerate,default 30 \n"
    "\t--fps    framerate,default 30 \n"
    "\t-i input file\n"
    "\t-o output file\n"
    "\t-?\n"
    "\t--help\n"
    "\tSet BMVPUENC_DISPLAY_FRAMERATE to view the details of fps:\n"
    "\texport BMVPUENC_DISPLAY_FRAMERATE = 0(default) shows the total fps info of all threads;\n"
    "\texport BMVPUENC_DISPLAY_FRAMERATE = 1 shows fps of every single thread;"
    "For example,\n"
    "\tbmvpuenc -w 426 -h 240 -i 240p.yuv -o 240p.265\n"
    "\tbmvpuenc -c 0 -w 426 -h 240 -i 240p.yuv -o 240p.264\n"
    "\tbmvpuenc -c 0 -f 1 -w 426 -h 240 -i 240p.nv12 -o 240p.264\n"
    ;

    fprintf(stderr, "Usage:\n\t%s [option]\n\noption:\n%s\n",
            progname, options);
    exit(0);
}

static int parse_args(int argc, char **argv, InputParameter* par)
{
    const struct option longOpts[] = {
        { "soc",     required_argument, NULL, 's' },
        { "pix_fmt", required_argument, NULL, 'f' },
        { "codec",   required_argument, NULL, 'c' },
        { "intra",   required_argument, NULL, 'p' }, // TODO
        { "gop_preset", required_argument, NULL, 'g' }, // TODO
        { "stride",  required_argument, NULL, 'y' },
        { "width",   required_argument, NULL, 'w' },
        { "height",  required_argument, NULL, 'h' }, // TODO
        { "loop",  required_argument, NULL, 'l' },
        { "run_times",  required_argument, NULL, 0 },
        { "fps",  required_argument, NULL, 'a' },
        { "help",    no_argument, NULL, 0 },
        { NULL,      no_argument, NULL, 0 }
    };
    int opt , ret;
    int longIndex = 0;

    memset(par, 0, sizeof(InputParameter));

    par->run_times = 1;
    par->thread_number = 1;
    par->enc.soc_idx = 0;
    par->enc.enc_fmt = 1;
    par->enc.gop_preset   = BM_VPU_ENC_GOP_PRESET_IBBBP;
    par->enc.intra_period = 28;
    par->enc.bit_rate = 0;
    par->enc.cqp = 32;
    par->enc.fps = 30;
    par->loop = 1;
    if (argc == 1) {
        /* No input argument */
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    while (1)
    {
        opt = getopt_long(argc, argv, "s:f:i:o:w:h:y:c:t:v:l:n:p:g:m:r:q:a:?", longOpts, &longIndex);
        if (opt == -1)
            break;
        switch (opt)
        {
        case 'v':
            par->log_level = atoi(optarg);
            break;
        case 'm':
            par->thread_number = atoi(optarg);
            break;
        case 's':
            par->enc.soc_idx = atoi(optarg);
            break;
        case 'c':
            par->enc.enc_fmt = atoi(optarg);
            break;
        case 'f':
            par->enc.pix_fmt = atoi(optarg);
            break;
        case 'p': // TODO
            par->enc.intra_period = atoi(optarg);
            break;
        case 'g': // TODO
            par->enc.gop_preset = atoi(optarg);
            break;
        case 'w':
            par->enc.crop_w = atoi(optarg);
            break;
        case 'h':
            par->enc.crop_h = atoi(optarg);
            break;
        case 'y':
            par->enc.y_stride = atoi(optarg);
            break;
        case 't':
            par->enc.aligned_height = atoi(optarg);
            break;
        case 'r':
            par->enc.bit_rate = atoi(optarg);
            break;
        case 'q':
            par->enc.cqp = atoi(optarg);
            break;
        case 'l':
            par->loop = atoi(optarg);
            break;
        case 'n':
            par->frame_number = atoi(optarg);
            break;
        case 'i':
            par->input_filename = optarg;
            break;
        case 'o':
            par->output_filename = optarg;
            break;
        case 'a':
            par->enc.fps = atoi(optarg);
            break;
        case '?':
            usage(argv[0]);
            return RETVAL_ERROR;
        case 0:
            ret = strcmp("help", longOpts[longIndex].name);
            if (ret == 0)
            {
                usage(argv[0]);
                return RETVAL_ERROR;
            }
            ret = strcmp("soc", longOpts[longIndex].name);
            if (ret == 0)
            {
                par->enc.soc_idx = atoi(optarg);
                break;
            }
            ret = strcmp("pix_fmt", longOpts[longIndex].name);
            if (ret == 0)
            {
                par->enc.pix_fmt = atoi(optarg);
                break;
            }
            ret = strcmp("codec", longOpts[longIndex].name);
            if (ret == 0)
            {
                par->enc.enc_fmt = atoi(optarg);
                break;
            }
            ret = strcmp("intra", longOpts[longIndex].name);
            if (ret == 0)
            {
                par->enc.intra_period = atoi(optarg);
                break;
            }
            ret = strcmp("gop_preset", longOpts[longIndex].name);
            if (ret == 0)
            {
                par->enc.gop_preset = atoi(optarg);
                break;
            }
            ret = strcmp("width", longOpts[longIndex].name);
            if (ret == 0)
            {
                par->enc.crop_w = atoi(optarg);
                break;
            }
            ret = strcmp("height", longOpts[longIndex].name);
            if (ret == 0)
            {
                par->enc.crop_h = atoi(optarg);
                break;
            }
            ret = strcmp("loop", longOpts[longIndex].name);
            if (ret == 0)
            {
                par->loop = atoi(optarg);
                break;
            }
            ret = strcmp("run_times", longOpts[longIndex].name);
            if (ret == 0)
            {
                par->run_times = atoi(optarg);
                break;
            }
            ret = strcmp("fps", longOpts[longIndex].name);
            if(ret == 0 ){
                par->enc.fps = atoi(optarg);
                break;
            }
            break;
        default:
            usage(argv[0]);
            return RETVAL_ERROR;
        }
    }

    if (par->enc.soc_idx < 0)
    {
        fprintf(stderr, "Error! invalid soc_idx (%d)\n",
                par->enc.soc_idx);
        usage(argv[0]);
        return RETVAL_ERROR;
    }

#if !defined(BM_PCIE_MODE)
    if (par->enc.soc_idx != 0)
    {
        fprintf(stderr, "[Warning] invalid soc_idx (%d), set soc_idx=0.\n",
                par->enc.soc_idx);
        par->enc.soc_idx = 0;
    }
#endif

    if (par->input_filename == NULL)
    {
        fprintf(stderr, "Missing input filename\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }
    if (par->output_filename == NULL)
    {
        fprintf(stderr, "Missing output filename\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->enc.enc_fmt < 0 || par->enc.enc_fmt > 1)
    {
        fprintf(stderr, "enc_fmt is NOT one of 0, 1\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->enc.pix_fmt < 0 || par->enc.pix_fmt > 1)
    {
        fprintf(stderr, "pix_fmt is NOT one of 0, 1\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->enc.crop_w <= 0)
    {
        fprintf(stderr, "width <= 0\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }
    if (par->enc.crop_h <= 0)
    {
        fprintf(stderr, "height <= 0\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->enc.y_stride == 0)
        par->enc.y_stride = par->enc.crop_w;
    else if (par->enc.y_stride < par->enc.crop_w)
    {
        fprintf(stderr, "luma stride is less than width\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->enc.y_stride%8)
    {
        fprintf(stderr, "luma stride is NOT multiple of 8\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->enc.aligned_height == 0)
        par->enc.aligned_height = par->enc.crop_h;
    else if (par->enc.aligned_height < par->enc.crop_h)
    {
        fprintf(stderr, "aligned height is less than actual height\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->enc.pix_fmt == 0)
        par->enc.c_stride = par->enc.y_stride/2;
    else
        par->enc.c_stride = par->enc.y_stride;

    if (par->enc.bit_rate < 0)
        par->enc.bit_rate = 0;

    if (par->enc.fps <= 0 )
        par->enc.fps = 30;

    if (par->enc.cqp < 0 || par->enc.cqp > 51)
    {
        fprintf(stderr, "Invalid quantization parameter %d\n", par->enc.cqp);
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->log_level < BMVPU_ENC_LOG_LEVEL_ERROR ||
        par->log_level > BMVPU_ENC_LOG_LEVEL_TRACE)
    {
        fprintf(stderr, "Wrong log level: %d\n", par->log_level);
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->thread_number < 1 ||
        par->thread_number > MAX_THREAD )
    {
        fprintf(stderr, "Wrong thread number: %d\n", par->thread_number);
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->loop < 0)
        par->loop = 1;

    if (par->run_times <= 0)
        par->run_times = 1;

    if (par->frame_number <= 0)
        par->frame_number = 0x7fffffff;

    return RETVAL_OK;
}

static int read_yuv_source(uint8_t* dst_va, int dst_stride_y, int dst_stride_c, int dst_height,
                           FILE**  src_file, int src_stride_y, int src_stride_c, int src_height,
                           int pix_format,
                           int width, int height)
{
    int dst_size_y = dst_stride_y * dst_height;
    int src_size_y = src_stride_y * src_height;
    int dst_height_c = dst_height/2; // TODO
    int src_height_c = src_height/2; // TODO
    int dst_size_c = dst_stride_c * dst_height_c;
    int src_size_c = src_stride_c * src_height_c;
    int log_level;
    int i;

    log_level = bmvpu_enc_get_logging_threshold();
    if (log_level > BMVPU_ENC_LOG_LEVEL_INFO)
    {
        printf("dst_stride_y=%d, dst_stride_c=%d, dst_height=%d\n", dst_stride_y, dst_stride_c, dst_height);
        printf("src_stride_y=%d, src_stride_c=%d, src_height=%d\n", src_stride_y, src_stride_c, src_height);
        printf("width=%d, height=%d\n", width, height);
    }

    if (src_stride_y < width)
    {
        fprintf(stderr, "src: Y stride(%d) < width(%d)\n", src_stride_y, width);
        return -1;
    }

    if (dst_stride_y < width)
    {
        fprintf(stderr, "dst: Y stride(%d) < width(%d)\n", dst_stride_y, width);
        return -1;
    }

    int dst_frame_size = dst_size_y + dst_size_c*2;
    if (pix_format == BM_VPU_ENC_PIX_FORMAT_NV12)
        dst_frame_size = dst_size_y + dst_size_c;

    if (dst_stride_y == src_stride_y &&
        dst_stride_c == src_stride_c &&
        dst_height   == src_height)
    {
        if ( dst_frame_size > fread(dst_va, sizeof(uint8_t), dst_frame_size, *src_file)){
            printf("eof when read in dst frame...\n");
            return -1;
        }

        return 0;
    }

    int src_frame_size = src_size_y + src_size_c*2;
    if (pix_format == BM_VPU_ENC_PIX_FORMAT_NV12)
        src_frame_size = src_size_y + src_size_c;
    uint8_t* tmp_buffer = malloc(sizeof(uint8_t)*src_frame_size);
    if (tmp_buffer==NULL)
    {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    if ( src_frame_size > fread(tmp_buffer, sizeof(uint8_t), src_frame_size, *src_file) ){
        printf("eof when read in source frame...\n");
        free(tmp_buffer);
        return -1;
    }

    /* Y */
    uint8_t* dy = dst_va;
    uint8_t* sy = tmp_buffer;
    for (i=0; i<height; i++)
    {
        memcpy(dy, sy, width);
        sy += src_stride_y;
        dy += dst_stride_y;
    }

    if (pix_format == BM_VPU_ENC_PIX_FORMAT_YUV420P)
    {
        int w_c = (width +1)/2;
        int h_c = (height+1)/2;

        /* U */
        int src_offset = src_size_y;
        int dst_offset = dst_size_y;
        dy = dst_va + dst_offset;
        sy = tmp_buffer + src_offset;
        for (i=0; i<h_c; i++)
        {
            memcpy(dy, sy, w_c);
            sy += src_stride_c;
            dy += dst_stride_c;
        }

        /* V */
        src_offset += src_size_c;
        dst_offset += dst_size_c;
        dy = dst_va + dst_offset;
        sy = tmp_buffer + src_offset;
        for (i=0; i<h_c; i++)
        {
            memcpy(dy, sy, w_c);
            sy += src_stride_c;
            dy += dst_stride_c;
        }
    }
    else
    {
        int w_c = (width +1)/2*2;
        int h_c = (height+1)/2;

        /* UV */
        int src_offset = src_size_y;
        int dst_offset = dst_size_y;
        dy = dst_va + dst_offset;
        sy = tmp_buffer + src_offset;
        for (i=0; i<h_c; i++)
        {
            memcpy(dy, sy, w_c);
            sy += src_stride_c;
            dy += dst_stride_c;
        }
    }

    free(tmp_buffer);

    return 0;
}

static BmVpuFramebuffer* get_src_framebuffer(VpuEncContext *ctx)
{
    BmVpuFramebuffer *fb = *((BmVpuFramebuffer**)bm_queue_pop(ctx->frame_unused_queue));
    int log_level;
    int i;

    if (fb==NULL)
    {
        fprintf(stderr, "frame buffer is NULL, pop\n");
        return NULL;
    }

    log_level = bmvpu_enc_get_logging_threshold();
    if (log_level > BMVPU_ENC_LOG_LEVEL_INFO)
    {
#ifdef __linux__
        printf("[%zx] myIndex = 0x%x, %p, pop\n", pthread_self(), fb->myIndex, fb);
#endif
#ifdef _WIN32
        printf("[%zx] myIndex = 0x%x, %p, pop\n", GetCurrentThreadId(), fb->myIndex, fb);
#endif
    }
    for (i=0; i<ctx->num_src_fb; i++)
    {
        if (&(ctx->src_fb_list[i]) == fb)
            return fb;
    }

    return NULL;
}

static void* sigmgr_thread(void* argument)
{
    threads_t* threads = (threads_t*)argument;
#ifdef __linux__
    siginfo_t  info;
    int        i, ret;
    while (1)  {
        ret = sigwaitinfo(&waitsigset, &info);
        if (ret != -1) {
            printf("\nfetch the signal %d\n", ret);
            if (info.si_signo == SIGINT || info.si_signo == SIGTERM) {
                for (i=0; i<threads->number; i++) {
                    pthread_t ptid = threads->handles[i];
                    if (ptid != NULL)
                    {
                        printf("Thread 0x%lx is canceling...\n", ptid);
                        pthread_cancel(ptid);
                        threads->handles[i] = NULL;
                    }
                }
            }
        } else {
            printf("\nsigwaitinfo() failed! errno %d: %s\n",
                   errno, strerror(errno));
        }
    }
#endif
    return NULL;
}

int main(int argc, char *argv[])
{
    InputParameter par = {0};
#ifdef __linux__
    sigset_t oldset;
#elif _WIN32
    // unwanted
#endif
    char tmp[256] = {0};
    int i, ret;

    ret = parse_args(argc, argv, &par);
    if (ret != RETVAL_OK)
    {
        return -1;
    }

    bmvpu_enc_set_logging_threshold(par.log_level);

    bmvpu_enc_set_logging_function(logging_fn);


#ifdef __linux__
    InputParameter mt_par[par.thread_number];
    memset(mt_par, 0, sizeof(InputParameter)*par.thread_number);

    sigemptyset(&waitsigset);
    sigaddset(&waitsigset, SIGINT);
    sigaddset(&waitsigset, SIGTERM);

    /* Block the SIGINT/SIGTERM signal.
     * The threads will inherit the signal mask.
     * This will avoid them catching SIGINT/SIGTERM. */
    pthread_sigmask(SIG_BLOCK, &waitsigset, &oldset);

    pthread_t thread_handle[par.thread_number];
#endif
#ifdef _WIN32
    InputParameter *mt_par = (InputParameter *)malloc(sizeof(InputParameter)*par.thread_number);
    memset(mt_par, 0, sizeof(InputParameter)*par.thread_number);
    const int arrayIndex = par.thread_number;
    HANDLE *thread_handle = (HANDLE *)malloc(sizeof(HANDLE) * arrayIndex);
#endif
    for (i=0; i<par.thread_number; i++)
    {
        memcpy(&(mt_par[i]), &par, sizeof(InputParameter));
        mt_par[i].thread_id = i;
#ifdef __linux__
        ret = pthread_create(&(thread_handle[i]), NULL, (void*)run, (void*)(&(mt_par[i])));
        if (ret < 0)
        {
            snprintf(tmp, 256, "Failed to create pthread #%d\n", i);
            handle_error(tmp);
        }
#endif
#ifdef _WIN32
        if ((thread_handle[i] = CreateThread(NULL, 0, (void*)run, (void*)(&(mt_par[i])), 0, NULL)) == NULL ) {
            printf("create thread error\n");
        }
#endif
        printf("Thread %d: %zx\n", i, thread_handle[i]);
    }

#ifdef WIN32
    thread_stat = CreateThread(NULL, 0, statPthread, (void*)(&(par)), 0, NULL);
    if (thread_stat == NULL) {
        printf("stat pthread create failed \n");
    }
#else
    ret = pthread_create(&(thread_stat), NULL, stat_pthread, (void*)(&(par)));
    if (ret != 0) {
        printf("stat pthread create failed \n");
    }
#endif

#ifdef __linux__
    pthread_t sigmgr_thread_id;
#endif
    threads_t threads = {0};

    threads.number  = par.thread_number;
    threads.handles = thread_handle;
#ifdef __linux__
    ret = pthread_create(&sigmgr_thread_id, NULL, sigmgr_thread, &threads);
    if (ret < 0)
    {
        snprintf(tmp, 256, "Failed to create sigmgr thread\n");
        handle_error(tmp);
    }
#endif
    /* Join threads. */
    for (i=0; i<par.thread_number; i++)
    {
#ifdef __linux__
        ret = pthread_join(thread_handle[i], NULL);
        if (ret < 0)
        {
            snprintf(tmp, 256, "Failed to join pthread #%d\n", i);
            handle_error(tmp);
        }
#elif _WIN32
        if (WaitForSingleObject(thread_handle[i], INFINITE) != WAIT_OBJECT_0) {
			printf("release thread error\n");
        }
#endif
    }
    printf("All threads have done, set g_exit_flag = 1 \n");
    g_exit_flag = 1;

#ifdef WIN32
        WaitForSingleObject(thread_stat, INFINITE);
#else
        pthread_join(thread_stat, NULL);
#endif

    for (i=0; i<par.thread_number; i++)
    {
        if (mt_par[i].result < 0)
        {
            snprintf(tmp, 256, "pthread #%d failed\n", i);
            handle_error(tmp);
        }
        printf("pthread #%d --- Done\n", i);
    }
#ifdef _WIN32
    if (thread_handle)
        free(thread_handle);
    if (mt_par)
        free(mt_par);
#endif
    /* Send a cancellation request to sigmgr thread */
#ifdef __linux__
    pthread_cancel(sigmgr_thread_id);

    ret = pthread_join(sigmgr_thread_id, NULL);
    if (ret < 0)
    {
        handle_error("Failed to join sigmgr thread\n");
    }

    /* Restore the old signal mask. */
    pthread_sigmask(SIG_SETMASK, &oldset, NULL);
#endif

    return 0;
}

