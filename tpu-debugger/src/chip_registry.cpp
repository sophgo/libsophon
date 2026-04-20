#include "chips/bm1688/bm1688.hpp"
#include "chips/bm1684x/bm1684x.hpp"
#include "chips/cv184x/cv184x.hpp"

namespace tpu_debugger {

// Register chips
REGISTER_CHIP(BM1688Chip)
REGISTER_CHIP(CV186XChip)
REGISTER_CHIP(BM1684XChip)
REGISTER_CHIP(CV184XChip)

} // namespace tpu_debugger
