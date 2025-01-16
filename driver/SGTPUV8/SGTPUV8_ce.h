#ifndef __CE_H__
#define __CE_H__

#define ce_err	pr_err
#define ce_warn	pr_warn

#define CE_DBG

#ifdef CE_DBG
#define ce_cont	pr_cont
#define ce_info pr_info
#define ce_dbg	pr_info
#define enter()	ce_info("enter %s\n", __func__)
#define line()	ce_info("%s %d\n", __FILE__, __LINE__)
#else
#define ce_cont(...)	do {} while (0)
#define ce_info(...)	do {} while (0)
#define ce_dbg(...)	do {} while (0)
#define enter(...)	do {} while (0)
#define line(...)	do {} while (0)
#endif

#define ce_assert(cond)	WARN_ON(!(cond))

typedef u32	ce_u32_t;
typedef u64	ce_u64_t;
typedef u8	ce_u8_t;

#define ce_readl(reg)		readl(&(reg))
#define ce_writel(reg, value)	writel(value, &(reg))
#define ce_setbit(reg, bit)	ce_writel(reg, ce_readl(reg) | (1 << bit))
#define ce_clrbit(reg, bit)	ce_writel(reg, ce_readl(reg) & ~(1 << bit))
#define ce_getbit(reg, bit)	((ce_readl(reg) >> bit) & 1)

enum {
	CE_EINVAL = 100,	/* invalid argument */
	CE_EALG,		/* invalid algrithm */
	CE_EBLKLEN,		/* not aligned to block length */
	CE_EKEYLEN,		/* key length not valid */
	CE_ESKEY,		/* secure key not valid, but requested */
	CE_ENULL,		/* null pointer */
	CE_ERR,
	CE_TIMEOUT,
};

//symmetric
enum {
	CE_DES_ECB = 0,
	CE_DES_CBC,
	CE_DES_CTR,
	CE_DES_EDE3_ECB,
	CE_DES_EDE3_CBC,
	CE_DES_EDE3_CTR,
	CE_AES_128_ECB,
	CE_AES_128_CBC,
	CE_AES_128_CTR,
	CE_AES_192_ECB,
	CE_AES_192_CBC,
	CE_AES_192_CTR,
	CE_AES_256_ECB,
	CE_AES_256_CBC,
	CE_AES_256_CTR,
	CE_SM4_ECB,
	CE_SM4_CBC,
	CE_SM4_CTR,
	CE_SM4_OFB,
	CE_CIPHER_MAX,
};

//hash
enum {
	CE_SHA1,
	CE_SHA256,
	CE_SM3,
	CE_HASH_MAX,
};

//base N
enum {
	CE_BASE64,
	CE_BASE_MAX,
};

struct ce_cipher {
	int             alg;
	//set 0 to decryption, otherwise encryption
	int		enc;
	void            *iv;
	void            *key;
	void            *src;
	void            *dst;
	unsigned long   len;
	void		*iv_out;
};

struct ce_hash {
	int			alg;
	void			*init;
	void			*src;
	unsigned long		len;
	void			*hash;
};

//Base N -- Base16 Base32 Base64
//Now only stdardard base64 is supported which defined in RFC4648 section 4
struct ce_base64 {
	int             alg;
	//set 0 to decryption, otherwise encryption
	int		enc;
	void            *src;
	void            *dst;
	unsigned long   len;
	//only used in decode mode, crypto engine is a dma device.
	//it accepts physicall address, so it cannot access data using cpu.
	//so the caller should tell me real output size.
	unsigned long	dstlen;
};

typedef enum ce_type {
	CE_CIPER=0,
	CE_HASH,
	CE_BASE
}ce_type_t;

struct spacc_batch {
	ce_type_t cetype;
	void* user_dst;

	union{
	struct ce_cipher ce_cipher_t;
	struct ce_hash ce_hash_t;
	struct ce_base64 ce_base_t;
	}u;
};

typedef struct spacc_drv_context {
	struct mutex spacc_mutex;
	wait_queue_head_t  wq_spacc;
	int got_event_spacc;
} spacc_drv_context_t;

static inline void dumpp(const void *p, const unsigned long l)
{
	unsigned long i;
	char buf[128];
	unsigned int count;

	if (p == NULL) {
		ce_info("null\n");
		return;
	}
	count = 0;
	for (i = 0; i < l; ++i) {
		count += snprintf(buf + count, sizeof(buf) - count,
				   "%02x ", ((unsigned char *)p)[i]);
		if (count % 64 == 0) {
			ce_info("%s\n", buf);
			count = 0;
		}
	}
	if (count)
		ce_info("%s\n", buf);
}

static inline void dump(const void *p, const unsigned long l)
{
	unsigned long i;
	char buf[128];
	unsigned int count;

	if (p == NULL) {
		ce_info("null\n");
		return;
	}
	count = 0;
	for (i = 0; i < l; ++i) {
		if (i % 16 == 0)
			count += snprintf(buf + count,
					   sizeof(buf) - count, "%04lx: ", i);

		count += snprintf(buf + count, sizeof(buf) - count,
				   "%02x ", ((unsigned char *)p)[i]);

		if (i % 16 == 15) {
			ce_info("%s\n", buf);
			count = 0;
		}
	}
	if (i % 16 != 15)
		ce_info("%s\n", buf);
}

//ce is short for CryptoEngine
int ce_cipher_do(void *dev, struct ce_cipher *cipher);
int ce_save_iv(void *dev, struct ce_cipher *cipher);
int ce_save_hash(void *dev, struct ce_hash *hash);
int ce_hash_do(void *dev, struct ce_hash *hash);
int ce_base_do(void *dev, struct ce_base64 *base);
unsigned int ce_int_status(void *dev);
void ce_int_clear(void *dev);
int ce_status(void *dev);
int ce_is_secure_key_valid(void *dev);
int ce_cipher_block_len(unsigned int alg);
int ce_hash_hash_len(unsigned int alg);
int ce_hash_block_len(unsigned int alg);
unsigned long ce_base_len(struct ce_base *base);
int ce_dmacpy(void *dev, void *dst, void *src, unsigned long len);

int bmdev_is_seckey_valid(struct bm_device_info *bmdi, unsigned long arg);
int trigger_spacc(struct bm_device_info *bmdi, unsigned long arg);
void bm_spacc_request_irq(struct bm_device_info *bmdi);
void bm_spacc_free_irq(struct bm_device_info *bmdi);
int spacc_init(struct bm_device_info *bmdi);
void spacc_exit(struct bm_device_info *bmdi);
extern int bmdev_memcpy_d2s(struct bm_device_info *bmdi,struct file *file,
		void __user *dst, u64 src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode);

#endif
