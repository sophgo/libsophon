#include "bmcnnctx.h"
#include "bmruntime.h"

using bmruntime::Bmruntime;

namespace bmcnn {

void bmcnn_ctx_destroy(bmcnn_ctx_t handle)
{
    delete (Bmruntime *)handle;
}

bmcnn_ctx_t bmcnn_ctx_create_by_devid(
    const std::string &ctx_dir, int devid,
    const std::string &chipname)
{
    Bmruntime *bmrt = new Bmruntime(chipname, devid);
    if (!bmrt->load_context(ctx_dir))
        return NULL;
    return bmrt;
}

bool bmcnn_ctx_append(const std::string &ctx_dir, void *bmrt)
{
    if (!((Bmruntime *)bmrt)->load_context(ctx_dir))
        return false;

    return true;
}

} /* namespace bmcnn */
