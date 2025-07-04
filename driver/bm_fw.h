#ifndef _BM_FW_H_
#define _BM_FW_H_

#define LAST_INI_REG_VAL	 0x76125438

struct bm_device_info;

enum fw_downlod_stage {
	FW_START = 0,
	DDR_INIT_DONE = 1,
	DL_DDR_IMG_DONE	= 2
};
enum arm9_fw_mode {
	FW_PCIE_MODE,
	FW_SOC_MODE,
	FW_MIX_MODE
};
typedef struct bm_firmware_desc {
	unsigned int *itcm_fw;	//bytes
	int itcmfw_size;
	unsigned int *ddr_fw;
	int ddrfw_size;		//bytes
} bm_fw_desc, *pbm_fw_desc;

struct firmware_header{
	char magic[4]; // 字符串"spfw"，sophon-firmware缩写，如果前四个字符不是这个，就按裸的firmware进行load, 其他信息全用0填充
	u8 major; // 主版本号
	u8 minor; // 次版本号
	u16 patch; // patch版本号
	u32 date; // YYMMDD--
	u32 commit_hash; // hash前8个十六进制，大端表示
	u32 chip_id; // 设备id
	u32 fw_size; // firmware数据大小
	u32 fw_crc32; // firmware实际数据校验码
	u8 fw_data[0]; // firmware实际数据
} ;

int bmdrv_fw_load(struct bm_device_info *bmdi, struct file *file, pbm_fw_desc fw);
void bmdrv_fw_unload(struct bm_device_info *bmdi);
int bmdrv_wait_fwinit_done(struct bm_device_info *bmdi);
#endif
