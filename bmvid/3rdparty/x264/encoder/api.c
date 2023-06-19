/*****************************************************************************
 * api.c: bit depth independent interface
 *****************************************************************************
 * Copyright (C) 2003-2020 x264 project
 *
 * Authors: Vittorio Giovara <vittorio.giovara@gmail.com>
 *          Luca Barbato <lu_zero@gentoo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/

#include "common/base.h"

/****************************************************************************
 * global symbols
 ****************************************************************************/
const int x264_chroma_format = X264_CHROMA_FORMAT;

x264_t *x264_8_encoder_open( x264_param_t * );
void x264_8_nal_encode( x264_t *h, uint8_t *dst, x264_nal_t *nal );
int  x264_8_encoder_reconfig( x264_t *, x264_param_t * );
void x264_8_encoder_parameters( x264_t *, x264_param_t * );
int  x264_8_encoder_headers( x264_t *, x264_nal_t **pp_nal, int *pi_nal );
int  x264_8_encoder_encode( x264_t *, x264_nal_t **pp_nal, int *pi_nal, x264_picture_t *pic_in, x264_picture_t *pic_out );
void x264_8_encoder_close( x264_t * );
int  x264_8_encoder_delayed_frames( x264_t * );
int  x264_8_encoder_maximum_delayed_frames( x264_t * );
void x264_8_encoder_intra_refresh( x264_t * );
int  x264_8_encoder_invalidate_reference( x264_t *, int64_t pts );

x264_t *x264_10_encoder_open( x264_param_t * );
void x264_10_nal_encode( x264_t *h, uint8_t *dst, x264_nal_t *nal );
int  x264_10_encoder_reconfig( x264_t *, x264_param_t * );
void x264_10_encoder_parameters( x264_t *, x264_param_t * );
int  x264_10_encoder_headers( x264_t *, x264_nal_t **pp_nal, int *pi_nal );
int  x264_10_encoder_encode( x264_t *, x264_nal_t **pp_nal, int *pi_nal, x264_picture_t *pic_in, x264_picture_t *pic_out );
void x264_10_encoder_close( x264_t * );
int  x264_10_encoder_delayed_frames( x264_t * );
int  x264_10_encoder_maximum_delayed_frames( x264_t * );
void x264_10_encoder_intra_refresh( x264_t * );
int  x264_10_encoder_invalidate_reference( x264_t *, int64_t pts );

typedef struct x264_api_t
{
    /* Internal reference to x264_t data */
    x264_t *x264;

    /* API entry points */
    void (*nal_encode)( x264_t *h, uint8_t *dst, x264_nal_t *nal );
    int  (*encoder_reconfig)( x264_t *, x264_param_t * );
    void (*encoder_parameters)( x264_t *, x264_param_t * );
    int  (*encoder_headers)( x264_t *, x264_nal_t **pp_nal, int *pi_nal );
    int  (*encoder_encode)( x264_t *, x264_nal_t **pp_nal, int *pi_nal, x264_picture_t *pic_in, x264_picture_t *pic_out );
    void (*encoder_close)( x264_t * );
    int  (*encoder_delayed_frames)( x264_t * );
    int  (*encoder_maximum_delayed_frames)( x264_t * );
    void (*encoder_intra_refresh)( x264_t * );
    int  (*encoder_invalidate_reference)( x264_t *, int64_t pts );
} x264_api_t;

REALIGN_STACK x264_t *x264_encoder_open( x264_param_t *param )
{
    x264_api_t *api = calloc( 1, sizeof( x264_api_t ) );
    if( !api )
        return NULL;

    if( HAVE_BITDEPTH8 && param->i_bitdepth == 8 )
    {
        api->nal_encode = x264_8_nal_encode;
        api->encoder_reconfig = x264_8_encoder_reconfig;
        api->encoder_parameters = x264_8_encoder_parameters;
        api->encoder_headers = x264_8_encoder_headers;
        api->encoder_encode = x264_8_encoder_encode;
        api->encoder_close = x264_8_encoder_close;
        api->encoder_delayed_frames = x264_8_encoder_delayed_frames;
        api->encoder_maximum_delayed_frames = x264_8_encoder_maximum_delayed_frames;
        api->encoder_intra_refresh = x264_8_encoder_intra_refresh;
        api->encoder_invalidate_reference = x264_8_encoder_invalidate_reference;

        api->x264 = x264_8_encoder_open( param );
    }
    else if( HAVE_BITDEPTH10 && param->i_bitdepth == 10 )
    {
        api->nal_encode = x264_10_nal_encode;
        api->encoder_reconfig = x264_10_encoder_reconfig;
        api->encoder_parameters = x264_10_encoder_parameters;
        api->encoder_headers = x264_10_encoder_headers;
        api->encoder_encode = x264_10_encoder_encode;
        api->encoder_close = x264_10_encoder_close;
        api->encoder_delayed_frames = x264_10_encoder_delayed_frames;
        api->encoder_maximum_delayed_frames = x264_10_encoder_maximum_delayed_frames;
        api->encoder_intra_refresh = x264_10_encoder_intra_refresh;
        api->encoder_invalidate_reference = x264_10_encoder_invalidate_reference;

        api->x264 = x264_10_encoder_open( param );
    }
    else
        x264_log_internal( X264_LOG_ERROR, "not compiled with %d bit depth support\n", param->i_bitdepth );

    if( !api->x264 )
    {
        free( api );
        return NULL;
    }

    /* x264_t is opaque */
    return (x264_t *)api;
}

REALIGN_STACK void x264_encoder_close( x264_t *h )
{
    x264_api_t *api = (x264_api_t *)h;

    api->encoder_close( api->x264 );
    free( api );
}

REALIGN_STACK void x264_nal_encode( x264_t *h, uint8_t *dst, x264_nal_t *nal )
{
    x264_api_t *api = (x264_api_t *)h;

    api->nal_encode( api->x264, dst, nal );
}

REALIGN_STACK int x264_encoder_reconfig( x264_t *h, x264_param_t *param)
{
    x264_api_t *api = (x264_api_t *)h;

    return api->encoder_reconfig( api->x264, param );
}

REALIGN_STACK void x264_encoder_parameters( x264_t *h, x264_param_t *param )
{
    x264_api_t *api = (x264_api_t *)h;

    api->encoder_parameters( api->x264, param );
}

REALIGN_STACK int x264_encoder_headers( x264_t *h, x264_nal_t **pp_nal, int *pi_nal )
{
    x264_api_t *api = (x264_api_t *)h;

    return api->encoder_headers( api->x264, pp_nal, pi_nal );
}

REALIGN_STACK int x264_encoder_encode( x264_t *h, x264_nal_t **pp_nal, int *pi_nal, x264_picture_t *pic_in, x264_picture_t *pic_out )
{
    x264_api_t *api = (x264_api_t *)h;

    return api->encoder_encode( api->x264, pp_nal, pi_nal, pic_in, pic_out );
}

REALIGN_STACK int x264_encoder_delayed_frames( x264_t *h )
{
    x264_api_t *api = (x264_api_t *)h;

    return api->encoder_delayed_frames( api->x264 );
}

REALIGN_STACK int x264_encoder_maximum_delayed_frames( x264_t *h )
{
    x264_api_t *api = (x264_api_t *)h;

    return api->encoder_maximum_delayed_frames( api->x264 );
}

REALIGN_STACK void x264_encoder_intra_refresh( x264_t *h )
{
    x264_api_t *api = (x264_api_t *)h;

    api->encoder_intra_refresh( api->x264 );
}

REALIGN_STACK int x264_encoder_invalidate_reference( x264_t *h, int64_t pts )
{
    x264_api_t *api = (x264_api_t *)h;

    return api->encoder_invalidate_reference( api->x264, pts );
}

#define MAX_NUM_NAL 16

typedef struct {
    int*        p_nnal;
    x264_nal_t* p_nal; // max slice count <= (10-3)
    uint8_t*    p_payload;
} bmx264_nalptr_t;


typedef struct {
    char func[64];
    int  par_num;
    long ret_val;
} bmx264_msg_header_t;

typedef struct {
    int  type; // 0: 64-bit pointer; 1: 32-bit signed integer; 2: 32-bit unsigned integer; 3: string
    int  size; // the number of bytes
    char name[16];
} bmx264_msg_par_t;

#define BMX264_MSG_T(N) \
typedef struct { \
    bmx264_msg_header_t header; \
    bmx264_msg_par_t    par[N]; \
} bmx264_msg##N##_t;

BMX264_MSG_T(1);
BMX264_MSG_T(2);
BMX264_MSG_T(3);
BMX264_MSG_T(4);

//void msp_log(uint32_t level, const char *format, ...);

REALIGN_STACK int x264_run_task(void* msg, int msg_size)
{
    char* msg_ptr = (char*)strtoll(msg, NULL, 16);
    bmx264_msg_header_t *header = (bmx264_msg_header_t *)msg_ptr;

    if (header == NULL)
        return -(1);

#define OPT(STR) else if(!strcmp(header->func, STR))
    if (0);
    OPT("x264_param_default") {
        bmx264_msg1_t *msg1 = (bmx264_msg1_t *)msg_ptr;
        void* par = (void*)strtoll(msg1->par[0].name, NULL, 16);

        if (par==NULL)
            return -1;

        x264_param_default((x264_param_t*)par);

        x264_log_internal(X264_LOG_ERROR, "x264_param_default\n");

        header->ret_val = 0;

        return 0;
    } OPT("x264_param_default_preset") {
        bmx264_msg3_t *msg3 = (bmx264_msg3_t *)msg_ptr;
        void* par = (void*)strtoll(msg3->par[0].name, NULL, 16);
        const char *preset = msg3->par[1].name;
        const char *tune   = msg3->par[2].name;

        if (par==NULL)
            return -1;

        if (strlen(preset)==0)
            preset = NULL;
        if (strlen(tune)==0)
            tune = NULL;

        header->ret_val = x264_param_default_preset((x264_param_t*)par, preset, tune);

        return 0;
    } OPT("x264_encoder_open") {
        bmx264_msg1_t *msg1 = (bmx264_msg1_t *)msg_ptr;
        void* par = (void*)strtoll(msg1->par[0].name, NULL, 16);

        if (par==NULL)
            return -1;

        //x264_log_internal(X264_LOG_ERROR, "=== bit depth %d ===\n", ((x264_param_t*)par)->i_bitdepth);

        x264_t* h = x264_encoder_open((x264_param_t*)par);

        header->ret_val = (long)h;

        return 0;
    } OPT("x264_encoder_close") {
        bmx264_msg1_t *msg1 = (bmx264_msg1_t *)msg_ptr;
        void* handle = (void*)strtoll(msg1->par[0].name, NULL, 16);

        if (handle==NULL)
            return -1;

        x264_encoder_close((x264_t*)handle);

        header->ret_val = 0;

        return 0;
    } OPT("x264_encoder_reconfig") {
        bmx264_msg2_t *msg2 = (bmx264_msg2_t *)msg_ptr;
        void* handle = (void*)strtoll(msg2->par[0].name, NULL, 16);
        void* par    = (void*)strtoll(msg2->par[1].name, NULL, 16);

        if (handle==NULL)
            return -1;
        if (par==NULL)
            return -2;

        header->ret_val = x264_encoder_reconfig((x264_t*)handle, (x264_param_t*)par);

        return 0;
    } OPT("x264_encoder_headers") {
        bmx264_msg2_t *msg2 = (bmx264_msg2_t *)msg_ptr;
        void*    handle  = (void*)   strtoll(msg2->par[0].name, NULL, 16);
        uint8_t* nal_out = (uint8_t*)strtoll(msg2->par[1].name, NULL, 16);

        if (handle==NULL)
            return -1;
        if (nal_out==NULL)
            return -2;

        x264_nal_t *nal = NULL;
        int i_nal = 0;

        int frame_size = x264_encoder_headers((x264_t*)handle, &nal, &i_nal);

        if (i_nal > MAX_NUM_NAL)
            return -i_nal;

        if (frame_size < 0)
            return -6;

        bmx264_nalptr_t np = {0};
        np.p_nnal    = (int*)nal_out;
        np.p_nal     = (x264_nal_t*)(nal_out + sizeof(int));
        np.p_payload = nal_out + sizeof(int) + MAX_NUM_NAL*sizeof(x264_nal_t);

        if (frame_size > 0) {
            *(np.p_nnal) = i_nal;
            memcpy(np.p_nal, nal, sizeof(x264_nal_t)*i_nal);
            memcpy(np.p_payload, nal->p_payload, frame_size);
        }

        header->ret_val = frame_size;

        return 0;
    } OPT("x264_encoder_encode") {
        bmx264_msg4_t *msg4 = (bmx264_msg4_t *)msg_ptr;
        void* handle  = (void*)strtoll(msg4->par[0].name, NULL, 16);
        uint8_t* nal_out = (uint8_t*)strtoll(msg4->par[1].name, NULL, 16);
        void* pic     = (void*)strtoll(msg4->par[2].name, NULL, 16);
        void* pic_out = (void*)strtoll(msg4->par[3].name, NULL, 16);

        if (handle==NULL)
            return -1;
        if (nal_out==NULL)
            return -2;
        if (pic_out==NULL)
            return -4;

        x264_nal_t *nal = NULL;
        int i_nal = 0;

        int frame_size = x264_encoder_encode((x264_t*)handle, &nal, &i_nal, pic, pic_out);

        if (i_nal > MAX_NUM_NAL)
            return -i_nal;

        if (frame_size < 0)
            return -6;

        bmx264_nalptr_t np = {0};
        np.p_nnal    = (int*)nal_out;
        np.p_nal     = (x264_nal_t*)(nal_out + sizeof(int));
        np.p_payload = nal_out + sizeof(int) + MAX_NUM_NAL*sizeof(x264_nal_t);


        if (frame_size > 0) {
            *(np.p_nnal) = i_nal;
            memcpy(np.p_nal, nal, sizeof(x264_nal_t)*i_nal);
            memcpy(np.p_payload, nal->p_payload, frame_size);
        }

        header->ret_val = frame_size;

        return 0;
    } OPT("x264_encoder_delayed_frames") {
        bmx264_msg1_t *msg1 = (bmx264_msg1_t *)msg_ptr;
        void* handle = (void*)strtoll(msg1->par[0].name, NULL, 16);

        if (handle==NULL)
            return -1;

        header->ret_val = x264_encoder_delayed_frames((x264_t*)handle);

        return 0;
    } else {
        return -1;
    }

    return 0;
}

