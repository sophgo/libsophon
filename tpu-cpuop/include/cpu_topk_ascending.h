#ifndef CPU_TOPK_ASCENDING_LAYER_H
#define CPU_TOPK_ASCENDING_LAYER_H
#include "cpu_topk.h"
namespace bmcpu {
class cpu_topk_ascendinglayer : public cpu_topklayer {
public:
    explicit cpu_topk_ascendinglayer() : cpu_topklayer(false) {}
    virtual string get_layer_name () const {
        return "TOPK_ASCENDING";
    }
};
} /* namespace bmcpu */
#endif // CPU_TOPK_ASCENDING_LAYER_H
