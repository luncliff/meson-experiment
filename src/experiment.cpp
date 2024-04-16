#include "experiment.hpp"

#include <dispatch/dispatch.h>
#include <dispatch/private.h>
#include <iostream>
#include <span>
#include <spanstream>
#include <string_view>
#include <vulkan/vulkan.hpp>

namespace experiment {

uint32_t get_build_version() { return 14; }

} // namespace experiment
