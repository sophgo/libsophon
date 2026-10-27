#ifndef _BM_FW_H_
#define _BM_FW_H_

#define LAST_INI_REG_VAL	 0x76125438
#define C906_0_PARK      0x104000000
#define C906_1_PARK      0x108000000
#define BM1688_C906_0_PARK      C906_0_PARK
#define BM1688_C906_1_PARK      C906_1_PARK
#define CV84X6_CA55_PCIE_PARK	0x24100000
#define CV84X6_C906_0_PARK      0x1040000000
#define CV84X6_C906_1_PARK      0x1050000000   // 256M
#define CV84X6_C906_2_PARK      0x1060000000
#define CV84X6_C906_3_PARK      0x1070000000 

struct file;

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

struct bm_fw_entry {
	const char *fw_name;
	u64         park_addr;
	int       (*post_load)(struct bm_device_info *bmdi);
	u32         oneshot;    /* bit flag: skip load+post_load if fw_oneshot_mask & this */
};

#define FW_ONESHOT_C2C  0x1

struct chip_fw_map {
	u32                    chip_id;
	const char            *chip_name;
	struct bm_fw_entry    *fw_entries;
	int                    fw_count;
};


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
