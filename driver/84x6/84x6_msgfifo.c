#include "bm_common.h"
#include "bm_io.h"
#include "84x6_irq.h"

int bm84x6_clear_msgirq(struct bm_device_info *bmdi, int core_id)
{
    return 0;
}

u32 bm84x6_pending_msgirq_cnt(struct bm_device_info *bmdi)
{
    return 1;
}
