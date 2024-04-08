#include "../bm_common.h"
#include "bm1684_vpp.tmh"

#define VPP_OK                             (0)
#define VPP_ERR                            (-1)
#define VPP_ERR_COPY_FROM_USER             (-2)
#define VPP_ERR_WRONG_CROPNUM              (-3)
#define VPP_ERR_INVALID_FD                 (-4)
#define VPP_ERR_INT_TIMEOUT                (-5)
#define VPP_ERR_INVALID_PA                 (-6)
#define VPP_ERR_INVALID_CMD                (-7)

#define VPP_ENOMEM                         (-12)
#define VPP_ERR_IDLE_BIT_MAP               (-256)
#define VPP_ERESTARTSYS                    (-512)

#define ALIGN16(x) ((x + 0xf) & (~0xf))
#define ALIGN(x, a)	(((x) + (a) - 1) & ~((a) - 1))
#define lower_32_bits(n) ((u32)(n))
#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))


u64 ion_region[2] = {0, 0};
u64 npu_reserved[2] = {0, 0};
u64 vpu_reserved[2] = {0, 0};

#if 0
/*Positive number * 1024*/
/*negative number * 1024, then Complement code*/
YPbPr2RGB, BT601
Y = 0.299 R + 0.587 G + 0.114 B
U = -0.1687 R - 0.3313 G + 0.5 B + 128
V = 0.5 R - 0.4187 G - 0.0813 B + 128
R = Y + 1.4018863751529200 (Cr-128)
G = Y - 0.345806672214672 (Cb-128) - 0.714902851111154 (Cr-128)
B = Y + 1.77098255404941 (Cb-128)

YCbCr2RGB, BT601
Y = 16  + 0.257 * R + 0.504 * g + 0.098 * b
Cb = 128 - 0.148 * R - 0.291 * g + 0.439 * b
Cr = 128 + 0.439 * R - 0.368 * g - 0.071 * b
R = 1.164 * (Y - 16) + 1.596 * (Cr - 128)
G = 1.164 * (Y - 16) - 0.392 * (Cb - 128) - 0.812 * (Cr - 128)
B = 1.164 * (Y - 16) + 2.016 * (Cb - 128)
#endif

s32 csc_matrix_list[CSC_MAX][12] = {
//YCbCr2RGB,BT601
{0x000004a8, 0x00000000, 0x00000662, 0xfffc8450,
	0x000004a8, 0xfffffe6f, 0xfffffcc0, 0x00021e4d,
	0x000004a8, 0x00000812, 0x00000000, 0xfffbaca8 },
//YPbPr2RGB,BT601
{0x00000400, 0x00000000, 0x0000059b, 0xfffd322d,
	0x00000400, 0xfffffea0, 0xfffffd25, 0x00021dd6,
	0x00000400, 0x00000716, 0x00000000, 0xfffc74bc },
//RGB2YCbCr,BT601
{ 0x107, 0x204, 0x64, 0x4000,
	0xffffff68, 0xfffffed6, 0x1c2, 0x20000,
	0x1c2, 0xfffffe87, 0xffffffb7, 0x20000 },
//YCbCr2RGB,BT709
{ 0x000004a8, 0x00000000, 0x0000072c, 0xfffc1fa4,
	0x000004a8, 0xffffff26, 0xfffffdde, 0x0001338e,
	0x000004a8, 0x00000873, 0x00000000, 0xfffb7bec },
//RGB2YCbCr,BT709
{ 0x000000bb, 0x00000275, 0x0000003f, 0x00004000,
	0xffffff99, 0xfffffea5, 0x000001c2, 0x00020000,
	0x000001c2, 0xfffffe67, 0xffffffd7, 0x00020000 },
//RGB2YPbPr,BT601
{ 0x132, 0x259, 0x74, 0,
	0xffffff54, 0xfffffead, 0x200, 0x20000,
	0x200, 0xfffffe54, 0xffffffad, 0x20000 },
//YPbPr2RGB,BT709
{ 0x00000400, 0x00000000, 0x0000064d, 0xfffcd9be,
	0x00000400, 0xffffff40, 0xfffffe21, 0x00014fa1,
	0x00000400, 0x0000076c, 0x00000000, 0xfffc49ed },
//RGB2YPbPr,BT709
{ 0x000000da, 0x000002dc, 0x0000004a, 0,
	0xffffff8b, 0xfffffe75, 0x00000200, 0x00020000,
	0x00000200, 0xfffffe2f, 0xffffffd1, 0x00020000 },
};

//static s32 __attribute__((unused)) check_ion_pa(u64 pa)
//{
//	s32 ret = VPP_OK;
//
//	if (pa > 0)
//	ret = ((pa >= ion_region[0]) && (pa <= (ion_region[0] + ion_region[1]))) ? VPP_OK : VPP_ERR;
//
//	return ret;
//}
//static s32 __attribute__((unused)) check_vpu_pa(u64 pa)
//{
//	s32 ret = VPP_OK;
//
//	if (pa > 0)
//	ret = ((pa >= vpu_reserved[0]) && (pa <= (vpu_reserved[0] + vpu_reserved[1]))) ? VPP_OK : VPP_ERR;
//
//	return ret;
//}
//static s32 __attribute__((unused)) check_npu_pa(u64 pa)
//{
//	s32 ret = VPP_OK;
//
//	if (pa > 0)
//	ret = ((pa >= npu_reserved[0]) && (pa <= (npu_reserved[0] + npu_reserved[1]))) ? VPP_OK : VPP_ERR;
//
//	return ret;
//}


static void vpp_reg_write(s8 core_id, struct bm_device_info *bmdi, u32 val, u32 offset)
{
	if (core_id == 0)
		vpp0_reg_write(bmdi, offset, val);
	else
		vpp1_reg_write(bmdi, offset, val);
}

static u32 vpp_reg_read(s8 core_id, struct bm_device_info *bmdi, u32 offset)
{
	u32 ret;

	if (core_id == 0)
		ret = vpp0_reg_read(bmdi, offset);
	else
		ret = vpp1_reg_read(bmdi, offset);
	return ret;
}

static s32 vpp_soft_rst(struct bm_device_info *bmdi, s8 core_id)
{
	u32 reg_read = 0;
	u32 active_check = 0;
	u32 count_value = 0;

	/*set dma_stop=1*/
	vpp_reg_write(core_id, bmdi, 0x00080008, VPP_CONTROL0);

	/*check dma_stop_active==1*/
	for (count_value = 0; count_value < 1000; count_value++)
	{
		reg_read = vpp_reg_read(core_id, bmdi, VPP_STATUS);
		if ((reg_read >> 9) & 0x1)
		{
			active_check++;
			if (active_check == 5)
			{
				break;
			}
		}
		bm_udelay(1);
	}
	if((1000 == count_value) && (5 != active_check))
	{
		TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"vpp_soft_rst failed\n");
		return VPP_ERR;
	}
	/*vpp soft reset*/
	vpp_reg_write(core_id, bmdi, 0x00020000, VPP_CONTROL0);

	bm_udelay(10);

	vpp_reg_write(core_id, bmdi, 0x00020002, VPP_CONTROL0);


/*deassert dma_stop and check dma_stop_active*/
	vpp_reg_write(core_id, bmdi, 0x00080000, VPP_CONTROL0);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"vpp_soft_rst done\n");

	return VPP_OK;
}


static s32 vpp_reg_dump(struct bm_device_info *bmdi, s8 core_id)
{
	u32 reg_value = 0;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"core_id: %d\n",core_id);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_VERSION);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_VERSION: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_CONTROL0);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_CONTROL0: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_STATUS);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_STATUS: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_INT_EN);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_INT_EN: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_INT_CLEAR);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_INT_CLEAR: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_INT_STATUS);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_INT_STATUS: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_INT_RAW_STATUS);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_INT_RAW_STATUS: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_CMD_BASE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_CMD_BASE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_CMD_BASE_EXT);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_CMD_BASE_EXT: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, MAP_CONV_TIMEOUT_CNT);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"MAP_CONV_TIMEOUT_CNT: 0x%x\n",reg_value);

	reg_value = vpp_reg_read(core_id, bmdi, VPP_DES_HEAD);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DES_HEAD: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_NEXT_CMD_BASE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_NEXT_CMD_BASE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, MAP_CONV_OFF_BASE_Y);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"MAP_CONV_OFF_BASE_Y: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, MAP_CONV_OFF_BASE_C);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"MAP_CONV_OFF_BASE_C: 0x%x\n",reg_value);

	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_CTRL);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_CTRL: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, MAP_CONV_OFF_STRIDE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"MAP_CONV_OFF_STRIDE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_CROP_ST);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_CROP_ST: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_CROP_SIZE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_CROP_SIZE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_RY_BASE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_RY_BASE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_GU_BASE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_GU_BASE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_BV_BASE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_BV_BASE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_STRIDE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_STRIDE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_RY_BASE_EXT);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_RY_BASE_EXT: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_GU_BASE_EXT);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_GU_BASE_EXT: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SRC_BV_BASE_EXT);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SRC_BV_BASE_EXT: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_CTRL);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_CTRL: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_CROP_ST);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_CROP_ST: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_CROP_SIZE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_CROP_SIZE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_RY_BASE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_RY_BASE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_GU_BASE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_GU_BASE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_BV_BASE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_BV_BASE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_STRIDE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_STRIDE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_RY_BASE_EXT);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_RY_BASE_EXT: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_GU_BASE_EXT);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_GU_BASE_EXT: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_DST_BV_BASE_EXT);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_DST_BV_BASE_EXT: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SCL_CTRL);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SCL_CTRL: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SCL_INIT);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SCL_INIT: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SCL_X);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SCL_X: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_SCL_Y);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"VPP_SCL_Y: 0x%x\n",reg_value);

	return 0;
}


void vpp_clear_int(struct bm_device_info *bmdi, s32 core_id)
{

	if (core_id == 0)
		vpp0_reg_write(bmdi, VPP_INT_CLEAR, 0xffffffff);
	else
		vpp1_reg_write(bmdi, VPP_INT_CLEAR, 0xffffffff);
}

void vpp_irq_handler(struct bm_device_info *bmdi, s32 core_id)
{
	KeSetEvent(&bmdi->vppdrvctx.wq_vpp[core_id], 0, FALSE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"vpp irq handler, core id %d\n",core_id);
}

static s32 vpp_check_hw_idle(s8 core_id, struct bm_device_info *bmdi)
{
	s32 status, val;
	s32 count = 0;

	while (1) {
		status = vpp_reg_read(core_id, bmdi, VPP_STATUS);
		val = (status>>8) & 0x1;
		if (val) // idle
			break;

		count++;
		if (count > 20000) { // 2 Sec
			TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"vpp is busy!!! status 0x%08x, core %d, device %d\n", status, core_id, bmdi->dev_index);
			return VPP_ERR;
		}
		bm_udelay(100);
	}

	count = 0;
	while (1) {
		status = vpp_reg_read(core_id, bmdi, VPP_INT_RAW_STATUS);
		if (status == 0x0)
			break;

		count++;
		if (count > 20000) { // 2 Sec
			TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"vpp raw status 0x%08x, core %d, device %d\n",
					status, core_id, bmdi->dev_index);
			return VPP_ERR;
		}
		bm_udelay(100);

		vpp_reg_write(core_id, bmdi, 0xffffffff, VPP_INT_CLEAR);
	}

	return VPP_OK;
}

static s32 vpp_setup_desc(s8 core_id, struct bm_device_info *bmdi,
	struct vpp_batch *batch, u64 *vpp_desc_pa)
{
	UNREFERENCED_PARAMETER(batch);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP, "%!FUNC! enter\n");

	if ((core_id != 0) && (core_id != 1))
		TraceEvents( TRACE_LEVEL_INFORMATION, TRACE_VPP, "core_id err in vpp_setup_desc, core_id %d, dev_index  %d\n",
			core_id, bmdi->dev_index);

	s32 ret;
	/*check vpp hw idle*/
	ret = vpp_check_hw_idle(core_id, bmdi);
	if (ret < 0) {
		TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"vpp_check_hw_idle failed! core_id %d, dev_index %d.\n", core_id, bmdi->dev_index);
		return ret;
	}

	/*set cmd list addr*/
	vpp_reg_write(core_id, bmdi, (u32)(vpp_desc_pa[0] & 0xffffffff), VPP_CMD_BASE);
	vpp_reg_write(core_id, bmdi, (u32)((vpp_desc_pa[0] >> 32) & 0x7), VPP_CMD_BASE_EXT);
#if 0
	u32 cmd_base;
	u32 cmd_base_ext;
	cmd_base = vpp_reg_read(core_id, bmdi, VPP_CMD_BASE);
	cmd_base_ext = vpp_reg_read(core_id, bmdi, VPP_CMD_BASE_EXT);
#endif

	/*set vpp0 and vpp1 as cmd list mode and interrupt mode*/
	vpp_reg_write(core_id, bmdi, 0x00040004, VPP_CONTROL0);//cmd list
	vpp_reg_write(core_id, bmdi, 0x03, VPP_INT_EN);//interruput mode : vpp_in

	/*start vpp hw work*/
	vpp_reg_write(core_id, bmdi, 0x00010001, VPP_CONTROL0);

	return VPP_OK;
}

static s32 vpp_prepare_cmd_list(struct bm_device_info *bmdi, struct vpp_batch *batch,
	u64 *vpp_desc_pa)
{
	s32 idx, srcUV = 0, dstUV = 0, ret = VPP_OK;
	s32 input_format, output_format, input_plannar, output_plannar;
	s32 src_csc_en = 0;
	s32 csc_mode = YCbCr2RGB_BT601;
	s32 scale_enable = 1;   //v3:0   v4:1
	u64 dst_addr;
	u64 dst_addr0;
	u64 dst_addr1;
	u64 dst_addr2;
	struct vpp_descriptor *pdes_vpp[VPP_CROP_NUM_MAX];

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"[VPPDRV] p_prepare_cmd_list\n");

	for (idx = 0; idx < batch->num; idx++) {

		struct vpp_cmd *cmd = (batch->cmd + idx);

		if (((upper_32_bits(cmd->src_addr0) & 0x7) != 0x3) &&
			((upper_32_bits(cmd->src_addr0) & 0x7) != 0x4)) {
			TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"pa,cmd->src_addr0  0x%llx, input must be on ddr1 or ddr2\n", cmd->src_addr0);
			ret = VPP_ERR_INVALID_PA;
		}


		pdes_vpp[idx] = (struct vpp_descriptor *)ExAllocatePool(PagedPool,sizeof(struct vpp_descriptor));
		if (pdes_vpp[idx] == NULL) {
			ret = VPP_ENOMEM;
			TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"pdes_vpp[%d] is NULL, dev_index  %d\n", idx, bmdi->dev_index);
			return ret;
		}


		input_format = cmd->src_format;
		input_plannar = cmd->src_plannar;
		output_format = cmd->dst_format;
		output_plannar = cmd->dst_plannar;

		src_csc_en = cmd->src_csc_en;

		if (cmd->src_uv_stride == 0) {
			srcUV = (((input_format == YUV420) || (input_format == YUV422)) && (input_plannar == 1))
				? (cmd->src_stride/2) : (cmd->src_stride);
		} else {
			srcUV = cmd->src_uv_stride;
		}
		if (cmd->dst_uv_stride == 0) {
			dstUV = (((output_format == YUV420) || (output_format == YUV422)) && (output_plannar == 1))
				? (cmd->dst_stride/2) : (cmd->dst_stride);
		} else {
			dstUV = cmd->dst_uv_stride;
		}

		if ((cmd->csc_type >= 0) && (cmd->csc_type < CSC_MAX))
			csc_mode = cmd->csc_type;

		/*  the first crop*/
		if (idx == (batch->num - 1)) {
			pdes_vpp[idx]->des_head = 0x300 + idx;
			pdes_vpp[idx]->next_cmd_base = 0x0;
		} else {
			pdes_vpp[idx]->des_head = 0x200 + idx;
			pdes_vpp[idx]->next_cmd_base = (u32)(vpp_desc_pa[idx + 1] & 0xffffffff);
			pdes_vpp[idx]->des_head |= (u32)(((vpp_desc_pa[idx + 1] >> 32) & 0x7) << 16);
		}

		if (cmd->csc_type == CSC_USER_DEFINED) {
			pdes_vpp[idx]->csc_coe00 = cmd->matrix.csc_coe00 & 0x1fff;
			pdes_vpp[idx]->csc_coe01 = cmd->matrix.csc_coe01  & 0x1fff;
			pdes_vpp[idx]->csc_coe02 = cmd->matrix.csc_coe02  & 0x1fff;
			pdes_vpp[idx]->csc_add0  = cmd->matrix.csc_add0  & 0x1fffff;
			pdes_vpp[idx]->csc_coe10 = cmd->matrix.csc_coe10  & 0x1fff;
			pdes_vpp[idx]->csc_coe11 = cmd->matrix.csc_coe11  & 0x1fff;
			pdes_vpp[idx]->csc_coe12 = cmd->matrix.csc_coe12  & 0x1fff;
			pdes_vpp[idx]->csc_add1  = cmd->matrix.csc_add1  & 0x1fffff;
			pdes_vpp[idx]->csc_coe20 = cmd->matrix.csc_coe20  & 0x1fff;
			pdes_vpp[idx]->csc_coe21 = cmd->matrix.csc_coe21  & 0x1fff;
			pdes_vpp[idx]->csc_coe22 = cmd->matrix.csc_coe22 & 0x1fff;
			pdes_vpp[idx]->csc_add2  = cmd->matrix.csc_add2 & 0x1fffff;
		} else {
		/*not change*/
			pdes_vpp[idx]->csc_coe00 = csc_matrix_list[csc_mode][0]  & 0x1fff;
			pdes_vpp[idx]->csc_coe01 = csc_matrix_list[csc_mode][1]  & 0x1fff;
			pdes_vpp[idx]->csc_coe02 = csc_matrix_list[csc_mode][2]  & 0x1fff;
			pdes_vpp[idx]->csc_add0  = csc_matrix_list[csc_mode][3]  & 0x1fffff;
			pdes_vpp[idx]->csc_coe10 = csc_matrix_list[csc_mode][4]  & 0x1fff;
			pdes_vpp[idx]->csc_coe11 = csc_matrix_list[csc_mode][5]  & 0x1fff;
			pdes_vpp[idx]->csc_coe12 = csc_matrix_list[csc_mode][6]  & 0x1fff;
			pdes_vpp[idx]->csc_add1  = csc_matrix_list[csc_mode][7]  & 0x1fffff;
			pdes_vpp[idx]->csc_coe20 = csc_matrix_list[csc_mode][8]  & 0x1fff;
			pdes_vpp[idx]->csc_coe21 = csc_matrix_list[csc_mode][9]  & 0x1fff;
			pdes_vpp[idx]->csc_coe22 = csc_matrix_list[csc_mode][10] & 0x1fff;
			pdes_vpp[idx]->csc_add2  = csc_matrix_list[csc_mode][11] & 0x1fffff;
		}

		/*src crop parameter*/
		pdes_vpp[idx]->src_ctrl = ((src_csc_en & 0x1) << 11)
			| ((cmd->src_plannar & 0x1) << 8)
			| ((cmd->src_endian & 0x1) << 5)
			| ((cmd->src_endian_a & 0x1) << 4)
			| (cmd->src_format & 0xf);

		pdes_vpp[idx]->src_crop_st = (cmd->src_axisX << 16) | cmd->src_axisY;
		pdes_vpp[idx]->src_crop_size = (cmd->src_cropW << 16) | cmd->src_cropH;
		pdes_vpp[idx]->src_stride = (cmd->src_stride << 16) | (srcUV & 0xffff);

		if ((cmd->src_fd0 == 0) && (cmd->src_addr0 != 0)) {/*src addr is pa*/
			pdes_vpp[idx]->src_ry_base = (u32)(cmd->src_addr0 & 0xffffffff);
			pdes_vpp[idx]->src_gu_base = (u32)(cmd->src_addr1 & 0xffffffff);
			pdes_vpp[idx]->src_bv_base = (u32)(cmd->src_addr2 & 0xffffffff);

			pdes_vpp[idx]->src_ry_base_ext = (u32)((cmd->src_addr0 >> 32) & 0x7);
			pdes_vpp[idx]->src_gu_base_ext = (u32)((cmd->src_addr1 >> 32) & 0x7);
			pdes_vpp[idx]->src_bv_base_ext = (u32)((cmd->src_addr2 >> 32) & 0x7);
		} else {/*src addr is not pa*/
			TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"%!FUNC!, src addr0 is not physical addr\n");
		}

		/*dst crop parameter*/
		pdes_vpp[idx]->dst_ctrl = ((cmd->dst_plannar & 0x1) << 8)
			| ((cmd->dst_endian & 0x1) << 5)
			| ((cmd->dst_endian_a & 0x1) << 4)
			| (cmd->dst_format & 0xf);

		pdes_vpp[idx]->dst_crop_st = (cmd->dst_axisX << 16) | cmd->dst_axisY;
		pdes_vpp[idx]->dst_crop_size = (cmd->dst_cropW << 16) | cmd->dst_cropH;
		pdes_vpp[idx]->dst_stride = (cmd->dst_stride << 16) | (dstUV & 0xffff);

		if ((cmd->dst_fd0 == 0) && (cmd->dst_addr0 != 0)) {/*dst addr is pa*/
			/*if dst fmr is rgbp and only channel 0 addr is valid, The data for the three channels*/
			/* are b, g, r; not the original order:r, g, b*/
			if ((output_format == RGB24) && ((cmd->dst_plannar & 0x1) == 0x1)
				&& (cmd->dst_addr1 == 0) && (cmd->dst_addr2 == 0)) {
				dst_addr = cmd->dst_addr0;
				dst_addr0 = dst_addr + 2 * (u64)cmd->dst_cropH * (u64)ALIGN16(cmd->dst_cropW);
				dst_addr1 = dst_addr + (u64)cmd->dst_cropH * (u64)ALIGN16(cmd->dst_cropW);
				dst_addr2 = dst_addr;

				pdes_vpp[idx]->dst_ry_base = lower_32_bits(dst_addr0);
				pdes_vpp[idx]->dst_gu_base = lower_32_bits(dst_addr1);
				pdes_vpp[idx]->dst_bv_base = lower_32_bits(dst_addr2);

				pdes_vpp[idx]->dst_ry_base_ext = (u32)(upper_32_bits(dst_addr0) & 0x7);
				pdes_vpp[idx]->dst_gu_base_ext = (u32)(upper_32_bits(dst_addr1) & 0x7);
				pdes_vpp[idx]->dst_bv_base_ext = (u32)(upper_32_bits(dst_addr2) & 0x7);
			} else {
				pdes_vpp[idx]->dst_ry_base = (u32)(cmd->dst_addr0 & 0xffffffff);
				pdes_vpp[idx]->dst_gu_base = (u32)(cmd->dst_addr1 & 0xffffffff);
				pdes_vpp[idx]->dst_bv_base = (u32)(cmd->dst_addr2 & 0xffffffff);

				pdes_vpp[idx]->dst_ry_base_ext = (u32)((cmd->dst_addr0 >> 32) & 0x7);
				pdes_vpp[idx]->dst_gu_base_ext = (u32)((cmd->dst_addr1 >> 32) & 0x7);
				pdes_vpp[idx]->dst_bv_base_ext = (u32)((cmd->dst_addr2 >> 32) & 0x7);
			}
		} else {/*dst addr is not pa*/
			TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"%!FUNC!, line : %!LINE!, dst addr0 is not physical addr\n");
		}

		/*scl_ctrl parameter*/
		pdes_vpp[idx]->scl_ctrl = ((cmd->ver_filter_sel & 0xf) << 12)
			| ((cmd->hor_filter_sel & 0xf) << 8)
			| (scale_enable & 0x1);
		pdes_vpp[idx]->scl_int = ((cmd->scale_y_init & 0x3fff) << 16)
			| (cmd->scale_x_init & 0x3fff);

		pdes_vpp[idx]->scl_x = (u32)(cmd->src_cropW * 16384 / cmd->dst_cropW);
		pdes_vpp[idx]->scl_y = (u32)(cmd->src_cropH * 16384 / cmd->dst_cropH);

		if (cmd->mapcon_enable == 1) {
			u64 comp_base_y = 0, comp_base_c = 0, offset_base_y = 0,
				offset_base_c = 0, map_conv_off_base_y = 0;
			u64 map_conv_off_base_c = 0, map_comp_base_y = 0, map_comp_base_c = 0;
			u64 frm_h = 0, height = 0, map_pic_height_y = 0, map_pic_height_c = 0, frm_w = 0,
				comp_stride_y = 0, comp_stride_c = 0;

			if ((cmd->src_fd0 == 0) && (cmd->src_addr0 != 0)) {
				offset_base_y = cmd->src_addr0;
				comp_base_y = cmd->src_addr1;
				offset_base_c = cmd->src_addr2;
				comp_base_c = cmd->src_addr3;
			} else {
				TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"%!FUNC!, line : %!LINE!, src addr0 is not physical addr\n");
			}

			frm_h = cmd->rows;
			height = ((frm_h + 15) / 16) * 4 * 16;
			map_conv_off_base_y = offset_base_y + (u64)(cmd->src_axisX / 256) * height * 2 +
								(u64)(cmd->src_axisY / 4) * 2 * 16;
			pdes_vpp[idx]->map_conv_off_base_y = (u32)(map_conv_off_base_y & 0xfffffff0);

			map_conv_off_base_c = offset_base_c + (u64)(cmd->src_axisX / 2 / 256) * height * 2 +
								(u64)(cmd->src_axisY / 2 / 2) * 2 * 16;
			pdes_vpp[idx]->map_conv_off_base_c = (u32)(map_conv_off_base_c & 0xfffffff0);

			pdes_vpp[idx]->src_ctrl =
				((1 & 0x1) << 12) | (0x0 << 20) | (0x0 << 16) | pdes_vpp[idx]->src_ctrl;//little endian
//((1 & 0x1) << 12) | (0xf << 20) | (0xf << 16) | pdes_vpp[idx]->src_ctrl;//big endian

			map_pic_height_y = ALIGN(frm_h, 16) & 0x3fff;
			map_pic_height_c = ALIGN(frm_h / 2, 8) & 0x3fff;
			pdes_vpp[idx]->map_conv_off_stride = (u32)((map_pic_height_y << 16) | map_pic_height_c);
			pdes_vpp[idx]->src_crop_st = ((cmd->src_axisX & 0x1fff) << 16) | (cmd->src_axisY & 0x1fff);
			pdes_vpp[idx]->src_crop_size = ((cmd->src_cropW & 0x3fff) << 16) | (cmd->src_cropH & 0x3fff);
			frm_w = cmd->cols;
			comp_stride_y = ALIGN(ALIGN(frm_w, 16) * 4, 32);
			map_comp_base_y = comp_base_y + (cmd->src_axisY / 4)*comp_stride_y;
			pdes_vpp[idx]->src_ry_base = (u32)(map_comp_base_y & 0xfffffff0);

			comp_stride_c = ALIGN(ALIGN(frm_w / 2, 16) * 4, 32);
			map_comp_base_c = comp_base_c + (cmd->src_axisY / 2 / 2) * comp_stride_c;
			pdes_vpp[idx]->src_gu_base = (u32)(map_comp_base_c & 0xfffffff0);
			pdes_vpp[idx]->src_stride = ((comp_stride_y & 0xfffffff0) << 16) | (comp_stride_c & 0xfffffff0);

			pdes_vpp[idx]->src_ry_base_ext =
				(u32)((((map_conv_off_base_y >> 32) & 0x7) << 8) | ((map_comp_base_y >> 32) & 0x7));
			pdes_vpp[idx]->src_gu_base_ext =
				(u32)((((map_conv_off_base_c >> 32) & 0x7) << 8) | ((map_comp_base_c >> 32) & 0x7));

		}

		//dump_des(batch, pdes, vpp_desc_pa);
		bmdev_memcpy_s2d_internal(bmdi, vpp_desc_pa[idx], (void *)pdes_vpp[idx], sizeof(struct vpp_descriptor));

		ExFreePool(pdes_vpp[idx]);

	}


	return ret;
}

static s32 vpp_get_core_id(struct bm_device_info *bmdi, s8 *core)
{
	if (bmdi->vppdrvctx.vpp_idle_bit_map >= 3) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPP, "[fatal err!]take sem, but two vpp core are busy, vpp_idle_bit_map = %lld\n", bmdi->vppdrvctx.vpp_idle_bit_map);
		return VPP_ERR_IDLE_BIT_MAP;
	}

	WdfSpinLockAcquire(bmdi->vppdrvctx.vpp_spinlock);

	if (0x0 == (bmdi->vppdrvctx.vpp_idle_bit_map & 0x1)) {
		*core = 0;
		bmdi->vppdrvctx.vpp_idle_bit_map = (bmdi->vppdrvctx.vpp_idle_bit_map | 0x1);
	} else if (0x0 == (bmdi->vppdrvctx.vpp_idle_bit_map & 0x2)) {
		*core = 1;
		bmdi->vppdrvctx.vpp_idle_bit_map = (bmdi->vppdrvctx.vpp_idle_bit_map | 0x2);
	} else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPP, "[fatal err!]Abnormal status, vpp_idle_bit_map = %lld\n", bmdi->vppdrvctx.vpp_idle_bit_map);
		WdfSpinLockRelease(bmdi->vppdrvctx.vpp_spinlock);
		return VPP_ERR_IDLE_BIT_MAP;
	}
	WdfSpinLockRelease(bmdi->vppdrvctx.vpp_spinlock);

	return VPP_OK;
}

static s32 vpp_free_core_id(struct bm_device_info *bmdi, s8 core_id)
{
	if ((core_id != 0) && (core_id != 1)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPP, "vpp abnormal status, vpp_idle_bit_map = %lld, core_id is %d\n", bmdi->vppdrvctx.vpp_idle_bit_map, core_id);
		return VPP_ERR_IDLE_BIT_MAP;
	}

	WdfSpinLockAcquire(bmdi->vppdrvctx.vpp_spinlock);
	bmdi->vppdrvctx.vpp_idle_bit_map = (bmdi->vppdrvctx.vpp_idle_bit_map &  (~(0x1 << core_id)));
	WdfSpinLockRelease(bmdi->vppdrvctx.vpp_spinlock);

	return VPP_OK;
}

static void dump_des(struct vpp_batch *batch, struct vpp_descriptor **pdes_vpp, dma_addr_t *des_paddr)
{
	s32 idx = 0;

	//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"bmdi->dev_index   is      %d\n", bmdi->dev_index);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"batch->num   is      %d\n", batch->num);
	for (idx = 0; idx < batch->num; idx++) {
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"des_paddr[%d]   0x%llx\n", idx, des_paddr[idx]);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->des_head   0x%x\n", idx, pdes_vpp[idx]->des_head);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->next_cmd_base   0x%x\n", idx, pdes_vpp[idx]->next_cmd_base);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->map_conv_off_base_y   0x%x\n", idx, pdes_vpp[idx]->map_conv_off_base_y);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->map_conv_off_base_c   0x%x\n", idx, pdes_vpp[idx]->map_conv_off_base_c);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_ctrl   0x%x\n", idx, pdes_vpp[idx]->src_ctrl);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->map_conv_off_stride   0x%x\n", idx, pdes_vpp[idx]->map_conv_off_stride);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_crop_st   0x%x\n", idx, pdes_vpp[idx]->src_crop_st);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_crop_size   0x%x\n", idx, pdes_vpp[idx]->src_crop_size);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_ry_base   0x%x\n", idx, pdes_vpp[idx]->src_ry_base);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_gu_base   0x%x\n", idx, pdes_vpp[idx]->src_gu_base);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_bv_base   0x%x\n", idx, pdes_vpp[idx]->src_bv_base);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_stride   0x%x\n", idx, pdes_vpp[idx]->src_stride);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_ry_base_ext   0x%x\n", idx, pdes_vpp[idx]->src_ry_base_ext);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_gu_base_ext   0x%x\n", idx, pdes_vpp[idx]->src_gu_base_ext);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->src_bv_base_ext   0x%x\n", idx, pdes_vpp[idx]->src_bv_base_ext);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_ctrl   0x%x\n", idx, pdes_vpp[idx]->dst_ctrl);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_crop_st   0x%x\n", idx, pdes_vpp[idx]->dst_crop_st);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_crop_size   0x%x\n", idx, pdes_vpp[idx]->dst_crop_size);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_ry_base   0x%x\n", idx, pdes_vpp[idx]->dst_ry_base);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_gu_base   0x%x\n", idx, pdes_vpp[idx]->dst_gu_base);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_bv_base   0x%x\n", idx, pdes_vpp[idx]->dst_bv_base);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_stride   0x%x\n", idx, pdes_vpp[idx]->dst_stride);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_ry_base_ext   0x%x\n", idx, pdes_vpp[idx]->dst_ry_base_ext);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_gu_base_ext   0x%x\n", idx, pdes_vpp[idx]->dst_gu_base_ext);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->dst_bv_base_ext   0x%x\n", idx, pdes_vpp[idx]->dst_bv_base_ext);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->scl_ctrl   0x%x\n", idx, pdes_vpp[idx]->scl_ctrl);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->scl_int   0x%x\n", idx, pdes_vpp[idx]->scl_int);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->scl_x   0x%x\n", idx, pdes_vpp[idx]->scl_x);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"pdes_vpp[%d]->scl_y   0x%x\n", idx, pdes_vpp[idx]->scl_y);
	}
}

void vpp_dump(struct vpp_batch *batch)
{
	struct vpp_cmd *cmd ;
	s32 i;
	for (i = 0; i < batch->num; i++) {
		cmd = (batch->cmd+i);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"batch->num    is  %d      \n", batch->num);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_format  0x%x\n", i, cmd->src_format);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_stride  0x%x\n", i, cmd->src_stride);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_uv_stride  0x%x\n", i, cmd->src_uv_stride);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_endian  0x%x\n", i, cmd->src_endian);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_endian_a  0x%x\n", i, cmd->src_endian_a);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_plannar  0x%x\n", i, cmd->src_plannar);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_fd0  0x%x\n", i, cmd->src_fd0);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_fd1  0x%x\n", i, cmd->src_fd1);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_fd2  0x%x\n", i, cmd->src_fd2);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_addr0  0x%llx\n", i, cmd->src_addr0);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_addr1  0x%llx\n", i, cmd->src_addr1);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_addr2  0x%llx\n", i, cmd->src_addr2);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_axisX  0x%x\n", i, cmd->src_axisX);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_axisY  0x%x\n", i, cmd->src_axisY);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_cropW  0x%x\n", i, cmd->src_cropW);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_cropH  0x%x\n", i, cmd->src_cropH);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_format  0x%x\n", i, cmd->dst_format);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_stride  0x%x\n", i, cmd->dst_stride);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_uv_stride  0x%x\n", i, cmd->dst_uv_stride);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_endian  0x%x\n", i, cmd->dst_endian);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_endian_a  0x%x\n", i, cmd->dst_endian_a);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_plannar  0x%x\n", i, cmd->dst_plannar);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_fd0  0x%x\n", i, cmd->dst_fd0);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_fd1  0x%x\n", i, cmd->dst_fd1);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_fd2  0x%x\n", i, cmd->dst_fd2);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_addr0  0x%llx\n", i, cmd->dst_addr0);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_addr1  0x%llx\n", i, cmd->dst_addr1);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_addr2  0x%llx\n", i, cmd->dst_addr2);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_axisX  0x%x\n", i, cmd->dst_axisX);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_axisY  0x%x\n", i, cmd->dst_axisY);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_cropW  0x%x\n", i, cmd->dst_cropW);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->dst_cropH  0x%x\n", i, cmd->dst_cropH);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_csc_en  0x%x\n", i, cmd->src_csc_en);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->hor_filter_sel  0x%x\n", i, cmd->hor_filter_sel);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->ver_filter_sel  0x%x\n", i, cmd->ver_filter_sel);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->scale_x_init  0x%x\n", i, cmd->scale_x_init);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->scale_y_init  0x%x\n", i, cmd->scale_y_init);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->csc_type  0x%x\n", i, cmd->csc_type);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->mapcon_enable  0x%x\n", i, cmd->mapcon_enable);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_fd3  0x%x\n", i, cmd->src_fd3);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->src_addr3  0x%llx\n", i, cmd->src_addr3);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->cols  0x%x\n", i, cmd->cols);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"cmd id %d, cmd->rows  0x%x\n", i, cmd->rows);
	}
	return;
}

s32 vpp_handle_setup(struct bm_device_info *bmdi, struct vpp_batch *batch)
{
	s32 ret = VPP_OK, idx = 0;
	u64 *vpp_desc_pa;
	s8 core_id = -1;
	NTSTATUS  status;
	LARGE_INTEGER li;

	li.QuadPart = WDF_REL_TIMEOUT_IN_MS(10000);
	struct reserved_mem_info *resmem_info = &bmdi->gmem_info.resmem_info;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"[VPPDRV] start vpp_handle_setup\n");

	u64 count = KeReadStateSemaphore(&bmdi->vppdrvctx.vpp_core_sem);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"The Semaphore count is %lld\n", count);
	KeWaitForSingleObject(&bmdi->vppdrvctx.vpp_core_sem, Executive, KernelMode, FALSE, NULL);

	ret = vpp_get_core_id(bmdi, &core_id);
	if (ret != VPP_OK)
		goto err1;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"resmem_info->vpp_addr: 0x%llx, core_id %d", resmem_info->vpp_addr,core_id);

	vpp_desc_pa = (u64 *)ExAllocatePool(PagedPool,VPP_CROP_NUM_MAX * sizeof(u64));
	if (vpp_desc_pa == NULL) {
		ret = VPP_ENOMEM;
		TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"vpp_desc_pa is NULL, dev_index  %d\n", bmdi->dev_index);
		goto err2;
	}

	for (idx = 0; idx < batch->num; idx++)
		vpp_desc_pa[idx] = resmem_info->vpp_addr + (u64)core_id * (512 << 10) + ((1 << 10) * (u64)idx);



	ret = vpp_prepare_cmd_list(bmdi, batch, vpp_desc_pa);
	if (ret != VPP_OK) {
		TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"vpp_prepare_cmd_list failed \n");
		goto  err3;
	}

	ret = vpp_setup_desc(core_id, bmdi, batch, vpp_desc_pa);
	if (ret != VPP_OK) {
		TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"vpp_setup_desc failed \n");
		goto  err3;
	}

	status = KeWaitForSingleObject(&bmdi->vppdrvctx.wq_vpp[core_id], Executive, KernelMode, FALSE, &li);
	if (status == STATUS_TIMEOUT) {
			TraceEvents(TRACE_LEVEL_ERROR,TRACE_VPP,"vpp timeout,status  %d,core_id %d,vpp_idle_bit_map %lld, dev_index  %d, vpp_desc_pa  %p\n",
					status, core_id, bmdi->vppdrvctx.vpp_idle_bit_map, bmdi->dev_index, vpp_desc_pa);
			vpp_dump(batch);
			vpp_reg_dump(bmdi,core_id);
			vpp_soft_rst(bmdi,core_id);
			ret = VPP_ERR_INT_TIMEOUT;
		}
	if((status == STATUS_ALERTED) || (status == STATUS_USER_APC))
	{
		TraceEvents(TRACE_LEVEL_INFORMATION,TRACE_VPP,"vpp signal_pending \n");
	}

	if(status == STATUS_SUCCESS)
	{
		TraceEvents(TRACE_LEVEL_INFORMATION,TRACE_VPP,"vpp run success \n");
	}

err3:
	ExFreePool(vpp_desc_pa);

err2:
	ret |= vpp_free_core_id(bmdi, core_id);

err1:
	KeReleaseSemaphore(&bmdi->vppdrvctx.vpp_core_sem, 0, 1, FALSE);

	return ret;
}


s32 trigger_vpp(struct bm_device_info *bmdi, _In_ WDFREQUEST Request,
				_In_ size_t OutputBufferLength,  _In_ size_t  InputBufferLength)
{
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);
	struct vpp_batch *batch;

	NTSTATUS Status = 0;
	size_t bufSize= 0;


	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"[VPPDRV] start trigger_vpp\n");

	Status = WdfRequestRetrieveInputBuffer(Request, sizeof(struct vpp_batch), &batch, &bufSize);
	if (!NT_SUCCESS(Status)) {
		WdfRequestCompleteWithInformation(Request, Status, 0);
		TraceEvents( TRACE_LEVEL_ERROR, TRACE_VPP, "trigger_vpp copy_from_user wrong: "
			"%d, sizeof(struct vpp_batch) total need %u, really copyied %llu,dev_index  %d\n",
			Status, sizeof(struct vpp_batch), bufSize, bmdi->dev_index);
		Status = VPP_ERR_COPY_FROM_USER;
		return Status;
	}


	//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP,"batch.num is  %d, bufSize %d,sizeof(batch)   %d,InputBufferLength   %d\n", batch->num,bufSize,sizeof(batch),InputBufferLength);

	if ((batch->num <= 0) || (batch->num > VPP_CROP_NUM_MAX)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPP, "wrong num, batch.num  %d, dev_index  %d\n", batch->num, bmdi->dev_index);
		Status = VPP_ERR_WRONG_CROPNUM;
		WdfRequestCompleteWithInformation(Request, STATUS_INVALID_PARAMETER, 0);
		return Status;
	}

	Status = vpp_handle_setup(bmdi, batch);
	if ((Status != VPP_OK) && (Status != VPP_ERESTARTSYS)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPP, "trigger_vpp ,vpp_handle_setup wrong, ret %d, line  %!LINE!, "
			"dev_index  %d\n", Status, bmdi->dev_index);
		WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL,0);
	}
	else{
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
	}
	return Status;
}

s32 vpp_init(struct bm_device_info *bmdi)
{
	s32 i;
	WDF_OBJECT_ATTRIBUTES	attributes;
	NTSTATUS				status;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP, "%!FUNC! enter\n");

	//vpp_reg_dump(bmdi,0);
	//vpp_reg_dump(bmdi,1);

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = bmdi->WdfDevice;
	status = WdfSpinLockCreate(&attributes, &bmdi->vppdrvctx.vpp_spinlock);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	KeInitializeSemaphore(&bmdi->vppdrvctx.vpp_core_sem, VPP_CORE_MAX, VPP_CORE_MAX);
	bmdi->vppdrvctx.vpp_idle_bit_map = 0;
	for (i = 0; i < VPP_CORE_MAX; i++)
		KeInitializeEvent(&bmdi->vppdrvctx.wq_vpp[i], SynchronizationEvent, FALSE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPP, "%!FUNC! exit\n");

	return 0;
}

void vpp_exit(struct bm_device_info *bmdi)
{
	UNREFERENCED_PARAMETER(bmdi);
	return;
}

static void bmdrv_vpp0_irq_handler(struct bm_device_info *bmdi)
{
	vpp_clear_int(bmdi, 0);
	vpp_irq_handler(bmdi,0);
}

static void bmdrv_vpp1_irq_handler(struct bm_device_info *bmdi)
{
	vpp_clear_int(bmdi, 1);
	vpp_irq_handler(bmdi, 1);
}

void bm_vpp_request_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_request_irq(bmdi, VPP0_IRQ_ID, bmdrv_vpp0_irq_handler);
	bmdrv_submodule_request_irq(bmdi, VPP1_IRQ_ID, bmdrv_vpp1_irq_handler);
}

void bm_vpp_free_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_free_irq(bmdi, VPP0_IRQ_ID);
	bmdrv_submodule_free_irq(bmdi, VPP1_IRQ_ID);
}

