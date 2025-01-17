#include "experiment.hpp"

#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if __has_include(<vulkan/vulkan.hpp>)
#include <vulkan/vulkan.hpp>
#endif

namespace experiment {

uint32_t get_build_version() { return 14; }

#if __has_include(<vulkan/vulkan.hpp>)
bool check_vulkan_available() noexcept {
    try {
        auto functions = std::make_unique<vk::DispatchLoaderDynamic>();
        vk::DynamicLoader loader{};
        if (loader.success() == false)
            return false;
        functions->init(loader);
        return true;
    } catch (const std::runtime_error &) {
        return false;
    }
}

bool check_vulkan_runtime(uint32_t &api_version, uint32_t &runtime_version) noexcept {
    api_version = VK_API_VERSION_1_3;
    runtime_version = 0;
    auto f = std::make_unique<vk::DispatchLoaderDynamic>();
    vk::DynamicLoader loader{};
    if (loader.success() == false)
        return false;
    f->init(loader);
    return f->vkEnumerateInstanceVersion(&runtime_version) == VK_SUCCESS;
}
#endif

} // namespace experiment
