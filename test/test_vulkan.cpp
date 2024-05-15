#include <gtest/gtest.h>

#include <array>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>
#include <winrt/Windows.Foundation.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>

struct VulkanDynamicTest : public testing::Test {
    vk::DynamicLoader loader{"vulkan-1.dll"};
    vk::DispatchLoaderDynamic dynamic;

    void SetUp() {
        ASSERT_TRUE(loader.success());
        ASSERT_NO_THROW(dynamic.init(loader));
    }
};

TEST_F(VulkanDynamicTest, enum_instance_layer_extensions) {
    auto layers = vk::enumerateInstanceLayerProperties(dynamic);
    ASSERT_FALSE(layers.empty());

    for (const vk::LayerProperties &lp : layers) {
        std::string name{lp.layerName.data(), strnlen(lp.layerName.data(), VK_MAX_EXTENSION_NAME_SIZE)};
        ASSERT_FALSE(name.empty());

        auto extensions = vk::enumerateInstanceExtensionProperties(name, dynamic);
        for (const vk::ExtensionProperties &ep : extensions) {
            std::string_view ename{ep.extensionName.data(),
                                   strnlen(ep.extensionName.data(), VK_MAX_EXTENSION_NAME_SIZE)};
            ASSERT_FALSE(ename.empty());
        }
    }
}

TEST_F(VulkanDynamicTest, create_instance) {
    ASSERT_EQ(dynamic.vkDestroyInstance, nullptr);
    std::vector<const char *> layers{"VK_LAYER_KHRONOS_synchronization2"};
    std::vector<const char *> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
#if defined(_DEBUG)
    layers.emplace_back("VK_LAYER_KHRONOS_validation");
#endif

    vk::InstanceCreateInfo info{};
    info.setPEnabledLayerNames(layers);
    info.setPEnabledExtensionNames(extensions);

    vk::Instance instance = vk::createInstance(info, nullptr, dynamic);
    ASSERT_TRUE(instance);

    // reinit to update function pointers
    dynamic.init(instance);
    ASSERT_NE(dynamic.vkDestroyInstance, nullptr);
    instance.destroy(nullptr, dynamic);
}

struct VulkanTest : public VulkanDynamicTest {
    vk::Instance instance{nullptr};

    void SetUp() {
        VulkanDynamicTest::SetUp();
        SetupInstance();
        dynamic.init(instance);
    }
    void TearDown() {
        ASSERT_NE(dynamic.vkDestroyInstance, nullptr);
        instance.destroy(nullptr, dynamic);
    }

  private:
    void SetupInstance() {
        std::vector<const char *> layers{"VK_LAYER_KHRONOS_synchronization2"};
        std::vector<const char *> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
#if defined(_DEBUG)
        layers.emplace_back("VK_LAYER_KHRONOS_validation");
#endif
        vk::InstanceCreateInfo info{};
        info.setPEnabledLayerNames(layers);
        info.setPEnabledExtensionNames(extensions);
        instance = vk::createInstance(info, nullptr, dynamic);
        ASSERT_TRUE(instance);
    }

  protected:
    // info for 3 queues, index 0 for graphics, index 1 for compute, index 2 for transfer
    void SetupQueueCreateInfos(vk::PhysicalDevice device, std::array<vk::DeviceQueueCreateInfo, 3> &queues) {
        auto props = device.getQueueFamilyProperties(dynamic);
        ASSERT_FALSE(props.empty());
        size_t max_queue_count = 0;
        for (uint32_t i = 0; i < props.size(); ++i) {
            if (auto count = props[i].queueCount; count > max_queue_count)
                max_queue_count = count;

            if (props[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                queues[0].setQueueFamilyIndex(i);
                queues[0].setQueueCount(props[i].queueCount);
            }
            if (props[i].queueFlags & vk::QueueFlagBits::eCompute) {
                queues[1].setQueueFamilyIndex(i);
                queues[1].setQueueCount(props[i].queueCount);
            }
            if (props[i].queueFlags & vk::QueueFlagBits::eTransfer) {
                queues[2].setQueueFamilyIndex(i);
                queues[2].setQueueCount(1);
            }
        }
        static std::vector<float> priorities{};
        priorities.resize(max_queue_count, 0.3f);
        for (auto &q : queues)
            q.setPQueuePriorities(priorities.data());

        ASSERT_NE(queues[0].queueCount, 0);
        ASSERT_NE(queues[1].queueCount, 0);
        ASSERT_EQ(queues[2].queueCount, 1);
    }
};

TEST_F(VulkanTest, enum_physical_devices) {
    auto devices = instance.enumeratePhysicalDevices(dynamic);
    ASSERT_FALSE(devices.empty());
    for (vk::PhysicalDevice p : devices) {
        std::array<vk::DeviceQueueCreateInfo, 3> queues{};
        SetupQueueCreateInfos(p, queues);

        vk::DeviceCreateInfo info{};
        info.setQueueCreateInfos(queues);

        vk::Device device = p.createDevice(info, nullptr, dynamic);
        ASSERT_TRUE(device);
        device.destroy(nullptr, dynamic);
    }
}

void GetHardwareAdapter(IDXGIFactory1 *factory, IDXGIAdapter1 **output,
                        DXGI_GPU_PREFERENCE preference = DXGI_GPU_PREFERENCE_MINIMUM_POWER) noexcept(false) {
    winrt::com_ptr<IDXGIFactory6> factory6 = nullptr;
    if (auto hr = factory->QueryInterface(factory6.put()); FAILED(hr))
        winrt::throw_hresult(hr);
    // ...
    winrt::com_ptr<IDXGIAdapter1> adapter = nullptr;
    for (UINT index = 0; SUCCEEDED(
             factory6->EnumAdapterByGpuPreference(index, preference, __uuidof(IDXGIAdapter1), adapter.put_void()));
         ++index) {
        DXGI_ADAPTER_DESC1 desc{};
        if (auto hr = adapter->GetDesc1(&desc); FAILED(hr))
            continue;
        // Don't select the Basic Render Driver adapter.
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
        // Support DirectX 11.1?
        if (SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_1, __uuidof(ID3D12Device), nullptr)))
            break;
        adapter = nullptr;
    }
    *output = nullptr;
    if (adapter)
        winrt::check_hresult(adapter->QueryInterface(output));
}

/// @see https://github.com/krOoze/Hello_Triangle/blob/dxgi_interop/src/WSI/DxgiWsi.h
/// @see https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgiresource1-createsharedhandle
struct SwapChainCreationTest : public testing::Test {
    winrt::com_ptr<IDXGIFactory4> factory = nullptr;
    winrt::com_ptr<IDXGIAdapter1> adapter = nullptr;

    void SetUp() {
        UINT flags = 0;
#if defined(_DEBUG)
        flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
        if (auto hr = CreateDXGIFactory2(flags, __uuidof(IDXGIFactory4), factory.put_void()); FAILED(hr))
            winrt::throw_hresult(hr);
        ASSERT_NO_THROW(GetHardwareAdapter(factory.get(), adapter.put()));
        ASSERT_TRUE(adapter);
    }
    void TearDown() {
        adapter = nullptr;
        factory = nullptr;
    }
};

TEST_F(SwapChainCreationTest, d3d12_device_hwnd_invalid) {
    winrt::com_ptr<ID3D12Device> d12_device = nullptr;
    ASSERT_EQ(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, //
                                __uuidof(ID3D12Device), d12_device.put_void()),
              S_OK);
    ASSERT_TRUE(d12_device);

    winrt::com_ptr<ID3D12CommandQueue> queue = nullptr;
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        ASSERT_EQ(d12_device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), queue.put_void()), S_OK);
    }

    winrt::com_ptr<IDXGISwapChain> swapchain = nullptr;
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferDesc.Width = desc.BufferDesc.Height = 256;
        desc.OutputWindow = NULL;
        auto hr = factory->CreateSwapChain(queue.get(), &desc, swapchain.put());
        EXPECT_NE(hr, S_OK) << winrt::to_string(winrt::hresult_error{hr}.message());
    }
}

TEST_F(SwapChainCreationTest, d3d11_device) {
    winrt::com_ptr<ID3D11Device> d3d11_device = nullptr;
    winrt::com_ptr<ID3D11DeviceContext> d3d11_device_context = nullptr;
    {
        UINT flags = 0;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        if (auto hr = D3D11CreateDevice(adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, //
                                        levels, 3, D3D11_SDK_VERSION,                           //
                                        d3d11_device.put(), &level, d3d11_device_context.put());
            FAILED(hr))
            GTEST_FAIL() << winrt::to_string(winrt::hresult_error{hr}.message());
    }

    winrt::com_ptr<IDXGISwapChain1> swapchain = nullptr;
    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = desc.Height = 256;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        if (auto hr = factory->CreateSwapChainForComposition(d3d11_device.get(), &desc, nullptr, swapchain.put());
            FAILED(hr))
            GTEST_FAIL() << winrt::to_string(winrt::hresult_error{hr}.message());
    }
    ASSERT_TRUE(swapchain);
}

struct VulkanSwapChainTest : public VulkanTest {
    winrt::com_ptr<ID3D11Device> d3d11_device = nullptr;
    winrt::com_ptr<ID3D11DeviceContext> d3d11_device_context = nullptr;
    winrt::com_ptr<IDXGISwapChain1> swapchain = nullptr;

    void SetUp() {
        VulkanTest::SetUp();

        winrt::com_ptr<IDXGIFactory4> factory = nullptr;
        if (auto hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), factory.put_void()); FAILED(hr))
            winrt::throw_hresult(hr);

        winrt::com_ptr<IDXGIAdapter1> adapter = nullptr;
        ASSERT_NO_THROW(GetHardwareAdapter(factory.get(), adapter.put()));
        ASSERT_TRUE(adapter);

        d3d11_device = nullptr;
        d3d11_device_context = nullptr;
        {
            UINT flags = 0;
#if defined(_DEBUG)
            flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
            D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
            D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
            if (auto hr = D3D11CreateDevice(adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, //
                                            levels, 3, D3D11_SDK_VERSION,                           //
                                            d3d11_device.put(), &level, d3d11_device_context.put());
                FAILED(hr))
                GTEST_FAIL() << winrt::to_string(winrt::hresult_error{hr}.message());
        }

        swapchain = nullptr;
        {
            DXGI_SWAP_CHAIN_DESC1 desc{};
            desc.Width = desc.Height = 256;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 2;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            if (auto hr = factory->CreateSwapChainForComposition(d3d11_device.get(), &desc, nullptr, swapchain.put());
                FAILED(hr))
                GTEST_FAIL() << winrt::to_string(winrt::hresult_error{hr}.message());
        }
        ASSERT_TRUE(swapchain);
    }

    void TearDown() {
        swapchain = nullptr;
        d3d11_device_context = nullptr;
        d3d11_device = nullptr;
        VulkanTest::TearDown();
    }
};

TEST_F(VulkanSwapChainTest, shared_handle) {
    // ...
}