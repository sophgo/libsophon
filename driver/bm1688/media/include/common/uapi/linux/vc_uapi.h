/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
 *
 * File Name: vc_drv_uapi.h
 * Description:
 */

#ifndef __VC_UAPI_H__
#define __VC_UAPI_H__

#define VC_DRV_ENCODER_DEV_NAME			"soph_vc_enc"
#define VC_DRV_DECODER_DEV_NAME			"soph_vc_dec"

#define VC_DRV_IOCTL_MAGIC				'V'
/* encoder ioctl */
#define DRV_VC_VENC_CREATE_CHN				_IO(VC_DRV_IOCTL_MAGIC, 0)
#define DRV_VC_VENC_DESTROY_CHN				_IO(VC_DRV_IOCTL_MAGIC, 1)
#define DRV_VC_VENC_RESET_CHN				_IO(VC_DRV_IOCTL_MAGIC, 2)
#define DRV_VC_VENC_START_RECV_FRAME		_IO(VC_DRV_IOCTL_MAGIC, 3)
#define DRV_VC_VENC_STOP_RECV_FRAME			_IO(VC_DRV_IOCTL_MAGIC, 4)
#define DRV_VC_VENC_QUERY_STATUS			_IO(VC_DRV_IOCTL_MAGIC, 5)
#define DRV_VC_VENC_SET_CHN_ATTR			_IO(VC_DRV_IOCTL_MAGIC, 6)
#define DRV_VC_VENC_GET_CHN_ATTR			_IO(VC_DRV_IOCTL_MAGIC, 7)
#define DRV_VC_VENC_GET_STREAM				_IO(VC_DRV_IOCTL_MAGIC, 8)
#define DRV_VC_VENC_RELEASE_STREAM			_IO(VC_DRV_IOCTL_MAGIC, 9)
#define DRV_VC_VENC_INSERT_USERDATA			_IO(VC_DRV_IOCTL_MAGIC, 10)
#define DRV_VC_VENC_SEND_FRAME				_IO(VC_DRV_IOCTL_MAGIC, 11)
#define DRV_VC_VENC_SEND_FRAMEEX			_IO(VC_DRV_IOCTL_MAGIC, 12)
#define DRV_VC_VENC_REQUEST_IDR				_IO(VC_DRV_IOCTL_MAGIC, 13)
#define DRV_VC_VENC_SET_ROI_ATTR			_IO(VC_DRV_IOCTL_MAGIC, 14)
#define DRV_VC_VENC_GET_ROI_ATTR			_IO(VC_DRV_IOCTL_MAGIC, 15)
#define DRV_VC_VENC_SET_H264_TRANS			_IO(VC_DRV_IOCTL_MAGIC, 16)
#define DRV_VC_VENC_GET_H264_TRANS			_IO(VC_DRV_IOCTL_MAGIC, 17)
#define DRV_VC_VENC_SET_H264_ENTROPY		_IO(VC_DRV_IOCTL_MAGIC, 18)
#define DRV_VC_VENC_GET_H264_ENTROPY		_IO(VC_DRV_IOCTL_MAGIC, 19)
#define DRV_VC_VENC_SET_H264_VUI			_IO(VC_DRV_IOCTL_MAGIC, 20)
#define DRV_VC_VENC_GET_H264_VUI			_IO(VC_DRV_IOCTL_MAGIC, 21)
#define DRV_VC_VENC_SET_H265_VUI			_IO(VC_DRV_IOCTL_MAGIC, 22)
#define DRV_VC_VENC_GET_H265_VUI			_IO(VC_DRV_IOCTL_MAGIC, 23)
#define DRV_VC_VENC_SET_JPEG_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 24)
#define DRV_VC_VENC_GET_JPEG_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 25)
#define DRV_VC_VENC_GET_RC_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 26)
#define DRV_VC_VENC_SET_RC_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 27)
#define DRV_VC_VENC_SET_REF_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 28)
#define DRV_VC_VENC_GET_REF_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 29)
#define DRV_VC_VENC_SET_H265_TRANS			_IO(VC_DRV_IOCTL_MAGIC, 30)
#define DRV_VC_VENC_GET_H265_TRANS			_IO(VC_DRV_IOCTL_MAGIC, 31)
#define DRV_VC_VENC_SET_FRAMELOST_STRATEGY	_IO(VC_DRV_IOCTL_MAGIC, 32)
#define DRV_VC_VENC_GET_FRAMELOST_STRATEGY	_IO(VC_DRV_IOCTL_MAGIC, 33)
#define DRV_VC_VENC_SET_SUPERFRAME_STRATEGY	_IO(VC_DRV_IOCTL_MAGIC, 34)
#define DRV_VC_VENC_GET_SUPERFRAME_STRATEGY	_IO(VC_DRV_IOCTL_MAGIC, 35)
#define DRV_VC_VENC_SET_CHN_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 36)
#define DRV_VC_VENC_GET_CHN_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 37)
#define DRV_VC_VENC_SET_MOD_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 38)
#define DRV_VC_VENC_GET_MOD_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 39)
#define DRV_VC_VENC_ATTACH_VBPOOL			_IO(VC_DRV_IOCTL_MAGIC, 40)
#define DRV_VC_VENC_DETACH_VBPOOL			_IO(VC_DRV_IOCTL_MAGIC, 41)
#define DRV_VC_VENC_SET_CUPREDICTION		_IO(VC_DRV_IOCTL_MAGIC, 42)
#define DRV_VC_VENC_GET_CUPREDICTION		_IO(VC_DRV_IOCTL_MAGIC, 43)
#define DRV_VC_VENC_CALC_FRAME_PARAM		_IO(VC_DRV_IOCTL_MAGIC, 44)
#define DRV_VC_VENC_SET_FRAME_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 45)
#define DRV_VC_VENC_GET_FRAME_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 46)
/* decoder ioctl */
#define DRV_VC_VDEC_CREATE_CHN				_IO(VC_DRV_IOCTL_MAGIC, 47)
#define DRV_VC_VDEC_DESTROY_CHN				_IO(VC_DRV_IOCTL_MAGIC, 48)
#define DRV_VC_VDEC_GET_CHN_ATTR			_IO(VC_DRV_IOCTL_MAGIC, 49)
#define DRV_VC_VDEC_SET_CHN_ATTR			_IO(VC_DRV_IOCTL_MAGIC, 50)
#define DRV_VC_VDEC_START_RECV_STREAM		_IO(VC_DRV_IOCTL_MAGIC, 51)
#define DRV_VC_VDEC_STOP_RECV_STREAM		_IO(VC_DRV_IOCTL_MAGIC, 52)
#define DRV_VC_VDEC_QUERY_STATUS			_IO(VC_DRV_IOCTL_MAGIC, 53)
#define DRV_VC_VDEC_RESET_CHN				_IO(VC_DRV_IOCTL_MAGIC, 54)
#define DRV_VC_VDEC_SET_CHN_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 55)
#define DRV_VC_VDEC_GET_CHN_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 56)
#define DRV_VC_VDEC_SEND_STREAM				_IO(VC_DRV_IOCTL_MAGIC, 57)
#define DRV_VC_VDEC_GET_FRAME				_IO(VC_DRV_IOCTL_MAGIC, 58)
#define DRV_VC_VDEC_RELEASE_FRAME			_IO(VC_DRV_IOCTL_MAGIC, 59)
#define DRV_VC_VDEC_ATTACH_VBPOOL			_IO(VC_DRV_IOCTL_MAGIC, 60)
#define DRV_VC_VDEC_DETACH_VBPOOL			_IO(VC_DRV_IOCTL_MAGIC, 61)
#define DRV_VC_VDEC_SET_MOD_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 62)
#define DRV_VC_VDEC_GET_MOD_PARAM			_IO(VC_DRV_IOCTL_MAGIC, 63)
#define DRV_VC_DECODE_H265_TEST				_IO(VC_DRV_IOCTL_MAGIC, 64)
#define DRV_VC_DECODE_H264_TEST				_IO(VC_DRV_IOCTL_MAGIC, 65)
#define DRV_VC_ENCODE_MAIN_TEST				_IO(VC_DRV_IOCTL_MAGIC, 66)
#define DRV_VC_ENC_DEC_JPEG_TEST			_IO(VC_DRV_IOCTL_MAGIC, 67)

/* encoder slice split */
#define DRV_VC_VENC_SET_H264_SLICE_SPLIT        _IO(VC_DRV_IOCTL_MAGIC, 68)
#define DRV_VC_VENC_GET_H264_SLICE_SPLIT	_IO(VC_DRV_IOCTL_MAGIC, 69)
#define DRV_VC_VENC_SET_H265_SLICE_SPLIT	_IO(VC_DRV_IOCTL_MAGIC, 70)
#define DRV_VC_VENC_GET_H265_SLICE_SPLIT	_IO(VC_DRV_IOCTL_MAGIC, 71)
/* encoder ioctl */
#define DRV_VC_VENC_GET_H264_Dblk		_IO(VC_DRV_IOCTL_MAGIC, 72)
#define DRV_VC_VENC_SET_H264_Dblk		_IO(VC_DRV_IOCTL_MAGIC, 73)
#define DRV_VC_VENC_GET_H265_Dblk		_IO(VC_DRV_IOCTL_MAGIC, 74)
#define DRV_VC_VENC_SET_H265_Dblk		_IO(VC_DRV_IOCTL_MAGIC, 75)

#define DRV_VC_VENC_SET_H264_INTRA_PRED		_IO(VC_DRV_IOCTL_MAGIC, 76)
#define DRV_VC_VENC_GET_H264_INTRA_PRED		_IO(VC_DRV_IOCTL_MAGIC, 77)
#define DRV_VC_VENC_ENC_JPEG_TEST			_IO(VC_DRV_IOCTL_MAGIC, 78)
#define DRV_VC_VDEC_DEC_JPEG_TEST			_IO(VC_DRV_IOCTL_MAGIC, 79)
#define DRV_VC_VDEC_FRAME_ADD_USER			_IO(VC_DRV_IOCTL_MAGIC, 80)

#define DRV_VC_VENC_SET_CUSTOM_MAP			_IO(VC_DRV_IOCTL_MAGIC, 81)
#define DRV_VC_VENC_GET_INTINAL_INFO		_IO(VC_DRV_IOCTL_MAGIC, 82)
#define DRV_VC_VENC_SET_H265_SAO			_IO(VC_DRV_IOCTL_MAGIC, 83)
#define DRV_VC_VENC_GET_H265_SAO			_IO(VC_DRV_IOCTL_MAGIC, 84)
#define DRV_VC_VENC_GET_HEADER				_IO(VC_DRV_IOCTL_MAGIC, 85)
#define DRV_VC_VENC_GET_EXT_ADDR			_IO(VC_DRV_IOCTL_MAGIC, 86)

#define DRV_VC_VCODEC_SET_CHN               _IO(VC_DRV_IOCTL_MAGIC, 100)
#define DRV_VC_VCODEC_GET_CHN               _IO(VC_DRV_IOCTL_MAGIC, 101)


#define DRV_VC_VENC_SET_MJPEG_PARAM            _IO(VC_DRV_IOCTL_MAGIC, 102)
#define DRV_VC_VENC_GET_MJPEG_PARAM            _IO(VC_DRV_IOCTL_MAGIC, 103)
#define DRV_VC_VENC_ENABLE_IDR                 _IO(VC_DRV_IOCTL_MAGIC, 104)
#define DRV_VC_VENC_SET_H265_PRED_UNIT         _IO(VC_DRV_IOCTL_MAGIC, 105)
#define DRV_VC_VENC_GET_H265_PRED_UNIT         _IO(VC_DRV_IOCTL_MAGIC, 106)
#define DRV_VC_VENC_SET_SEARCH_WINDOW          _IO(VC_DRV_IOCTL_MAGIC, 107)
#define DRV_VC_VENC_GET_SEARCH_WINDOW          _IO(VC_DRV_IOCTL_MAGIC, 108)

#define DRV_VC_VDEC_SET_STRIDE_ALIGN           _IO(VC_DRV_IOCTL_MAGIC, 109)
#define DRV_VC_VDEC_SET_USER_PIC               _IO(VC_DRV_IOCTL_MAGIC, 110)
#define DRV_VC_VDEC_ENABLE_USER_PIC            _IO(VC_DRV_IOCTL_MAGIC, 111)
#define DRV_VC_VDEC_DISABLE_USER_PIC           _IO(VC_DRV_IOCTL_MAGIC, 112)
#define DRV_VC_VDEC_SET_DISPLAY_MODE           _IO(VC_DRV_IOCTL_MAGIC, 113)

#define DRV_VC_VENC_SET_EXTERN_BUF             _IO(VC_DRV_IOCTL_MAGIC, 114)
#define DRV_VC_VENC_ALLOC_PHYSICAL_MEMORY      _IO(VC_DRV_IOCTL_MAGIC, 115)
#define DRV_VC_VENC_FREE_PHYSICAL_MEMORY       _IO(VC_DRV_IOCTL_MAGIC, 116)

#endif /* __VC_UAPI_H__ */
