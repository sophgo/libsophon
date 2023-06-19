#include <bmlib_runtime.h>
#include <bmlib_internal.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#define ISL68127_SET_DMA_ADDR_CMD	0xf7
#define ISL68127_GET_DMA_DATA_CMD	0xf5
#define ISL68127_GET_DEVICE_ID_CMD	0xad
#define ISL68127_GET_REVERSION_ID_CMD	0xae
#define ISL68127_APPLY_SETTINGS_CMD	0xe7

#define ISL68127_NVM_SLOT_NUM_REG	0x1080
#define ISL68127_FULL_POWER_MODE_ENABLE	0x1095
#define ISL68127_VOLTAGE_REGULATION_ENABLE	0x106d
#define ISL68127_CHECK_PROGRAM_RESULT	0x1400
#define ISL68127_BANK_STATUS_REG	0x1401

void dump(void *s, int len);
int isl68127_set_dma_addr(bm_handle_t *handle, int bus, int slave_addr, uint16_t addr);
int isl68127_get_dma_data(bm_handle_t *handle, int bus, int slave_addr, uint16_t addr,
					 unsigned int len, uint8_t *data);
unsigned int isl68127_get_nvm_slot_num(bm_handle_t *handle, int bus, int slave_addr);
unsigned int isl68127_enable_full_power_mode(bm_handle_t *handle, int bus, int slave_addr);
unsigned int isl68127_enable_voltage_regulation(bm_handle_t *handle, int bus, int slave_addr);
unsigned int isl68127_check_program_result(bm_handle_t *handle, int bus, int slave_addr);
unsigned int isl68127_get_bank_status_reg(bm_handle_t *handle, int bus, int slave_addr);
unsigned int isl68127_get_device_id(bm_handle_t *handle, int bus, int slave_addr);
unsigned int isl68127_get_reversion_id(bm_handle_t *handle, int bus, int slave_addr);
int isl68127_program(bm_handle_t *handle, int bus, int addr, char *name);
static int hex2bin(char hex);
static inline __s8 i2c_smbus_read_i2c_block_data(bm_handle_t *handle, int bus, int slave_addr,
			 __u8 command, __u8 length, __u8 *values);
static inline __s32 i2c_smbus_write_i2c_block_data(bm_handle_t *handle, int bus, int slave_addr,
			 __u8 command, __u8 length, __u8 *values);

int main(int argc, char *argv[])
{
	bm_handle_t handle = NULL;
	bm_status_t ret = BM_SUCCESS;
	bm_i2c_smbus_ioctl_info i2c_buf;
	int chip_num = 0;
	int bus = 0;
	int isl68127_addr = 0x5c;

	ret = bm_dev_request(&handle, chip_num);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	printf("availiable nvm slots: %u\n", isl68127_get_nvm_slot_num(&handle, bus, isl68127_addr));

	isl68127_get_device_id(&handle, 0, 0x5c);

	isl68127_get_reversion_id(&handle, 0, 0x5c);

	// isl68127_enable_full_power_mode(&handle, bus, isl68127_addr);
	isl68127_enable_voltage_regulation(&handle, bus, isl68127_addr);

	isl68127_program(&handle, bus, isl68127_addr, argv[1]);

	printf("[chip0] isl68127 have change config!\n");

	ret = bm_dev_request(&handle, 1);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	printf("availiable nvm slots: %u\n", isl68127_get_nvm_slot_num(&handle, bus, isl68127_addr));

	isl68127_get_device_id(&handle, 0, 0x5c);

	isl68127_get_reversion_id(&handle, 0, 0x5c);

	isl68127_enable_voltage_regulation(&handle, bus, isl68127_addr);

	isl68127_program(&handle, bus, isl68127_addr, argv[1]);

	printf("[chip1] isl68127 have change config!\n");

	ret = bm_dev_request(&handle, 2);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	printf("availiable nvm slots: %u\n", isl68127_get_nvm_slot_num(&handle, bus, isl68127_addr));

	isl68127_get_device_id(&handle, 0, 0x5c);

	isl68127_get_reversion_id(&handle, 0, 0x5c);

	isl68127_enable_voltage_regulation(&handle, bus, isl68127_addr);

	isl68127_program(&handle, bus, isl68127_addr, argv[1]);

	printf("[chip2] isl68127 have change config!\n");
	return 0;
}

void dump(void *s, int len)
{
	int i;

	for (i = 0; i < len; ++i)
		printf("%02x ", ((uint8_t *)s)[i]);
	printf("\n");
}

int isl68127_set_dma_addr(bm_handle_t *handle, int bus, int slave_addr, uint16_t addr)
{
	uint8_t tmp[2];

	tmp[0] = addr & 0xff;
	tmp[1] = (addr >> 8) & 0xff;

	return i2c_smbus_write_i2c_block_data(handle, bus, slave_addr, ISL68127_SET_DMA_ADDR_CMD, sizeof(tmp), tmp);
}

int isl68127_get_dma_data(bm_handle_t *handle, int bus, int slave_addr, uint16_t addr,
             unsigned int len, uint8_t *data)
{
	int err;

	err = isl68127_set_dma_addr(handle, bus, slave_addr, addr);
	if (err) {
		fprintf(stderr, "failed to set dma address\n");
		return err;
	}
	return i2c_smbus_read_i2c_block_data(handle, bus, slave_addr, ISL68127_GET_DMA_DATA_CMD, len, data);
}

unsigned int isl68127_get_nvm_slot_num(bm_handle_t *handle, int bus, int slave_addr)
{
	uint8_t tmp[4];

	isl68127_get_dma_data(handle, bus, slave_addr, ISL68127_NVM_SLOT_NUM_REG, sizeof(tmp), tmp);

	return tmp[0] | (tmp[1] << 8) | (tmp[2] << 16) | (tmp[3] << 24);
}

unsigned int isl68127_enable_full_power_mode(bm_handle_t *handle, int bus, int slave_addr)
{
	uint8_t tmp[4];
	uint8_t data[2];
	data[0] = 0x01;
	data[1] = 0x00;
	int err;

	isl68127_get_dma_data(handle, bus, slave_addr, ISL68127_FULL_POWER_MODE_ENABLE, sizeof(tmp), tmp);
	printf("\rfp mode reg read=");
	dump(tmp, sizeof(tmp));
	tmp[2] &= 0x1f;
	tmp[3] &= 0xfe;
	printf("\rfp mode reg write=");
	dump(tmp, sizeof(tmp));
	err = i2c_smbus_write_i2c_block_data(handle, bus, slave_addr, ISL68127_GET_DMA_DATA_CMD, sizeof(tmp), tmp);
	if (err){
		printf("err = %d\n", err);
	}else {
		err = i2c_smbus_write_i2c_block_data(handle, bus, slave_addr, ISL68127_APPLY_SETTINGS_CMD, sizeof(data), data);
	}

	return err;
}

unsigned int isl68127_enable_voltage_regulation(bm_handle_t *handle, int bus, int slave_addr)
{
	uint8_t tmp[4];
	uint8_t data[2];
	data[0] = 0x01;
	data[1] = 0x00;
	int err;

	isl68127_get_dma_data(handle, bus, slave_addr, ISL68127_VOLTAGE_REGULATION_ENABLE, sizeof(tmp), tmp);
	printf("\rvoltage regulation read=");
	dump(tmp, sizeof(tmp));
	tmp[0] |= (1 << 0);
	printf("\rvoltage regulation write=");
	dump(tmp, sizeof(tmp));
	err = i2c_smbus_write_i2c_block_data(handle, bus, slave_addr, ISL68127_GET_DMA_DATA_CMD, sizeof(tmp), tmp);
	if (err){
		printf("err = %d\n", err);
	}else {
		err = i2c_smbus_write_i2c_block_data(handle, bus, slave_addr, ISL68127_APPLY_SETTINGS_CMD, sizeof(data), data);
	}

	return err;

}

unsigned int isl68127_check_program_result(bm_handle_t *handle, int bus, int slave_addr)
{
	uint8_t tmp[4];
	int err;

	err = isl68127_get_dma_data(handle, bus, slave_addr, ISL68127_CHECK_PROGRAM_RESULT, sizeof(tmp), tmp);
	if (err){
		printf("get program status failed\n");
		return 0;
	}
	printf("\rprogram status reg= ");
	dump(tmp, sizeof(tmp));

	return tmp[0];
}

unsigned int isl68127_get_bank_status_reg(bm_handle_t *handle, int bus, int slave_addr)
{
	uint8_t tmp[4];
	int err;

	err = isl68127_get_dma_data(handle, bus, slave_addr, ISL68127_BANK_STATUS_REG, sizeof(tmp), tmp);
	if (err){
		printf("get bank status failed\n");
		return err;
	}
	printf("\rbank status reg = ");
	dump(tmp, sizeof(tmp));

	return 0;
}

unsigned int isl68127_get_device_id(bm_handle_t *handle, int bus, int slave_addr)
{
	uint8_t tmp[5];
	int err;

	err = i2c_smbus_read_i2c_block_data(handle, bus, slave_addr, ISL68127_GET_DEVICE_ID_CMD, sizeof(tmp), tmp);

	if(err){
		printf("get error(%d) device id\n", err);
	}
	printf("\rIC_DEVICE_ID =");
	dump(tmp, sizeof(tmp));

	return err;
}

unsigned int isl68127_get_reversion_id(bm_handle_t *handle, int bus, int slave_addr)
{
	uint8_t tmp[5];
	int err;

	err = i2c_smbus_read_i2c_block_data(handle, bus, slave_addr, ISL68127_GET_REVERSION_ID_CMD, sizeof(tmp), tmp);

	printf("\rIC_DEVICE_REV =");
	dump(tmp, sizeof(tmp));

	return err;
}

static int hex2bin(char hex)
{
	if (hex >= '0' && hex <= '9')
		return hex - '0';
	else if (hex >= 'a' && hex <= 'f')
		return hex - 'a' + 10;
	else if (hex >= 'A' && hex <= 'F')
		return hex - 'A' + 10;
	else
		return -EINVAL;
}

int isl68127_program(bm_handle_t *handle, int bus, int addr, char *name)
{
	struct stat st;
	int err;
	char *hex;
	ssize_t size;
	int status_reg;

	err = stat(name, &st);
	if (err) {
		fprintf(stderr, "cannot get file status\n");
		return err;
	}

	hex = (char *)malloc(st.st_size + 1);

	if (!hex) {
		fprintf(stderr, "allocate buffer failed\n");
		return -ENOMEM;
	}

	int fd = open(name, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "cannot open file %s\n", name);
		return fd;
	}

	size = read(fd, hex, st.st_size);
	close(fd);
	if (size != st.st_size) {
		fprintf(stderr, "cannot get enough data\n");
		goto free_buf;
	}

	hex[st.st_size] = 0;

	/* printf("%s\n", hex); */

	int i, j;
	int tag, nb, cmd;//, pa, crc;
	uint8_t line_buf[64];
	int line_size;
	uint8_t data_buf[32];
	uint8_t write_buf[32];
	int data_size;

	for (i = 0; i < st.st_size; ++i) {
		for (line_size = 0; i < st.st_size; ++i) {
			if (line_size > sizeof(line_buf)) {
				fprintf(stderr, "hex string too long\n");
				goto free_buf;
			}

			if (hex[i] == '\n')
				break;
			else if (isxdigit(hex[i]))
				line_buf[line_size++] = hex[i];
		}

		/* empty line */
		if (line_size == 0)
			continue;

#if 0
		printf("L: ");
		for (int i = 0; i < line_size; ++i)
			printf("%c", line_buf[i]);
		printf("\n");
#endif

		if (line_size % 2) {
			fprintf(stderr, "wrong hex number per line\n");
			goto free_buf;
		}

		/* convert hex string to binary */
		for (j = 0; j < line_size; j += 2) {
			data_buf[j / 2] = hex2bin(line_buf[j]) << 4 |
				hex2bin(line_buf[j + 1]);
		}
		data_size = line_size / 2;

		if (data_size < 2) {
			fprintf(stderr, "wrong format: line too short\n");
			goto free_buf;
		}

		tag = data_buf[0];
		nb = data_buf[1];

		if (nb + 2 != data_size) {
			fprintf(stderr, "wrong format: invalid byte count\n");
			goto free_buf;
		}

		if (tag != 0)
			/* skip lines that shouldnot burn into device */
			continue;

		if (nb < 3) {
			fprintf(stderr, "wrong format: invalid byte count\n");
			goto free_buf;
		}

		// pa = data_buf[2];
		cmd = data_buf[3];

		/* ignore, we donnot use PEC */
		// crc = data_buf[data_size - 1];

		// printf("%02X", tag);
		// printf("%02X", nb);
		// printf("%02X", pa);
		// printf("%02X", cmd);

		for (int i = 0; i < nb - 3; ++i){
			// printf("%02X", data_buf[4 + i]);
			write_buf[i] = data_buf[4 + i];
		}

		// printf("%02X", crc);
		// printf("\n");

		i2c_smbus_write_i2c_block_data(handle, bus, addr, cmd, nb - 3, write_buf);

	}

	usleep(2000000);
	status_reg = isl68127_check_program_result(handle, bus, addr);
	if (status_reg & (1 << 0))
		printf("Program seccess\n");
	else
		printf("Program failed! status reg= 0x%x\n", status_reg);

	isl68127_get_bank_status_reg(handle, bus, addr);

free_buf:
	free(hex);

	return 0;
}

static inline __s8 i2c_smbus_read_i2c_block_data(bm_handle_t *handle, int bus, int slave_addr,
                         __u8 command, __u8 length, __u8 *values)
{
	if (length > 32)
		length = 32;

	struct bm_i2c_smbus_ioctl_info i2c_buf;
	unsigned char data[length];
	int i, err;

	for(i = 0; i < length; i++){
		i2c_buf.data[i] = 0;
	}

	i2c_buf.i2c_slave_addr = slave_addr;
	i2c_buf.i2c_index = bus;
	i2c_buf.i2c_cmd = command;
	i2c_buf.length = length;
	i2c_buf.op = 1;

	err = bm_handle_i2c_access(*handle, &i2c_buf);
	if (err == 0){
		for (i = 0; i < length; i++)
			values[i] = i2c_buf.data[i];
	} else
		return err;

	return 0;
}
static inline __s32 i2c_smbus_write_i2c_block_data(bm_handle_t *handle, int bus, int slave_addr,
                         __u8 command, __u8 length, __u8 *values)
{
	if (length > 32)
		length = 32;

	struct bm_i2c_smbus_ioctl_info i2c_buf;
	int i, err;

	for (i = 0; i < length; i++){
		i2c_buf.data[i] = values[i];
	}
	i2c_buf.i2c_slave_addr = slave_addr;
	i2c_buf.i2c_index = bus;
	i2c_buf.i2c_cmd = command;
	i2c_buf.length = length;
	i2c_buf.op = 0;

	err = bm_handle_i2c_access(*handle, &i2c_buf);

	if(err)
		printf("i2c smbus write failed[err=%d]\n", err);

	return err;

}
