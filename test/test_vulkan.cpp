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
        std::vector<const char *> extensions{VK_KHR_SURFACE_EXTENSION_NAME,
                                             VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME};
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

    vk::Device MakeDefaultDevice(vk::PhysicalDeviceMemoryProperties &props) {
        auto devices = instance.enumeratePhysicalDevices(dynamic);
        for (vk::PhysicalDevice p : devices) {
            std::array<vk::DeviceQueueCreateInfo, 3> queues{};
            SetupQueueCreateInfos(p, queues);

            vk::DeviceCreateInfo info{};
            info.setQueueCreateInfos(queues);
            std::initializer_list<const char *> required{
                VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,          //
                VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,    // ...
                VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,     //
                VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME //
            };
            info.setPEnabledExtensionNames(required);
            props = p.getMemoryProperties(dynamic);
            return p.createDevice(info, nullptr, dynamic);
        }
        return nullptr;
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
        std::initializer_list<const char *> required{
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,          //
            VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,    // ...
            VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,     //
            VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME //
        };
        info.setPEnabledExtensionNames(required);

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
    winrt::com_ptr<ID3D12Device> d3d12_device = nullptr;
    ASSERT_EQ(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, //
                                __uuidof(ID3D12Device), d3d12_device.put_void()),
              S_OK);
    ASSERT_TRUE(d3d12_device);

    winrt::com_ptr<ID3D12CommandQueue> queue = nullptr;
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        ASSERT_EQ(d3d12_device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), queue.put_void()), S_OK);
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

TEST_F(SwapChainCreationTest, d3d12_device) {
    winrt::com_ptr<ID3D12Device> d3d12_device = nullptr;
    ASSERT_EQ(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, //
                                __uuidof(ID3D12Device), d3d12_device.put_void()),
              S_OK);
    ASSERT_TRUE(d3d12_device);

    winrt::com_ptr<ID3D12CommandQueue> d3d12_queue = nullptr;
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        ASSERT_EQ(d3d12_device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), d3d12_queue.put_void()), S_OK);
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
        if (auto hr = factory->CreateSwapChainForComposition(d3d12_queue.get(), &desc, nullptr, swapchain.put());
            FAILED(hr))
            GTEST_FAIL() << winrt::to_string(winrt::hresult_error{hr}.message());
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
    vk::Device device = nullptr;
    vk::PhysicalDeviceMemoryProperties pmemory{};
    winrt::com_ptr<ID3D12Device> d3d12_device = nullptr;
    winrt::com_ptr<ID3D12CommandQueue> d3d12_queue = nullptr;
    winrt::com_ptr<IDXGISwapChain1> swapchain = nullptr;
    winrt::com_ptr<IDXGISwapChain4> swapchain4 = nullptr;

    void SetUp() {
        VulkanTest::SetUp();
        device = MakeDefaultDevice(pmemory);
        ASSERT_TRUE(device);

        winrt::com_ptr<IDXGIFactory4> factory = nullptr;
        if (auto hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), factory.put_void()); FAILED(hr))
            winrt::throw_hresult(hr);

        winrt::com_ptr<IDXGIAdapter1> adapter = nullptr;
        ASSERT_NO_THROW(GetHardwareAdapter(factory.get(), adapter.put()));
        ASSERT_TRUE(adapter);

        ASSERT_EQ(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, //
                                    __uuidof(ID3D12Device), d3d12_device.put_void()),
                  S_OK);
        ASSERT_TRUE(d3d12_device);

        {
            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            ASSERT_EQ(d3d12_device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), d3d12_queue.put_void()),
                      S_OK);
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
            if (auto hr = factory->CreateSwapChainForComposition(d3d12_queue.get(), &desc, nullptr, swapchain.put());
                FAILED(hr))
                GTEST_FAIL() << winrt::to_string(winrt::hresult_error{hr}.message());
        }
        ASSERT_TRUE(swapchain);
        ASSERT_EQ(swapchain->QueryInterface(swapchain4.put()), S_OK);
    }

    void TearDown() {
        swapchain4 = nullptr;
        swapchain = nullptr;
        d3d12_queue = nullptr;
        d3d12_device = nullptr;
        if (device)
            device.destroy(nullptr, dynamic);
        VulkanTest::TearDown();
    }
};

TEST_F(VulkanSwapChainTest, d3d12_shared_handle) {
    try {
        size_t count = 0;
        {
            DXGI_SWAP_CHAIN_DESC1 desc{};
            ASSERT_EQ(swapchain4->GetDesc1(&desc), S_OK);
            count = desc.BufferCount;
        }
        ASSERT_NE(count, 0);

        std::vector<HANDLE> sharings(count);
        std::vector<vk::Image> images(count);
        std::vector<vk::DeviceMemory> memories(count);
        for (auto i = 0u; i < count; ++i) {
            winrt::com_ptr<ID3D12Resource> buffer = nullptr;
            ASSERT_EQ(swapchain4->GetBuffer(i, __uuidof(ID3D12Resource), buffer.put_void()), S_OK);

            {
                D3D12_RESOURCE_DESC desc = buffer->GetDesc();
                vk::ExternalMemoryImageCreateInfo info1{};
                info1.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eD3D12Resource;
                vk::ImageCreateInfo info2{};
                info2.setPNext(&info1); // D3D12Resource
                info2.setImageType(vk::ImageType::e2D);
                info2.setMipLevels(1);
                info2.setArrayLayers(1);
                info2.setSamples(vk::SampleCountFlagBits::e1); // VK_SAMPLE_COUNT_1_BIT?
                info2.setFormat(vk::Format::eR8G8B8A8Unorm);
                info2.setExtent({static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height), 1});
                info2.setTiling(vk::ImageTiling::eOptimal);
                info2.setUsage(vk::ImageUsageFlagBits::eColorAttachment);
                info2.setSharingMode(vk::SharingMode::eExclusive);
                info2.setInitialLayout(vk::ImageLayout::eUndefined);
                images[i] = device.createImage(info2, nullptr, dynamic);
                ASSERT_TRUE(images[i]);
            }

            if (auto hr = d3d12_device->CreateSharedHandle(buffer.get(), nullptr, GENERIC_ALL, nullptr, &sharings[i]);
                FAILED(hr))
                throw winrt::hresult_error{hr};

            vk::MemoryWin32HandlePropertiesKHR ext{};
            ASSERT_EQ(device.getMemoryWin32HandlePropertiesKHR(vk::ExternalMemoryHandleTypeFlagBits::eD3D12Resource,
                                                               sharings[i], &ext, dynamic),
                      vk::Result::eSuccess);

            vk::MemoryRequirements reqs = device.getImageMemoryRequirements(images[i], dynamic);
            if (ext.memoryTypeBits == 0)
                ext.memoryTypeBits = reqs.memoryTypeBits;

            uint32_t index = UINT32_MAX;
            for (uint32_t i = 0; i < pmemory.memoryTypeCount; ++i) {
                const uint32_t bit = 0b0001 << i;
                if (ext.memoryTypeBits != bit)
                    continue;
                if (pmemory.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
                    index = i;
                if (index != UINT32_MAX)
                    break;
            }
            ASSERT_NE(index, UINT32_MAX);

            {
                vk::MemoryDedicatedAllocateInfo info1{};
                info1.setImage(images[i]);

                vk::ImportMemoryWin32HandleInfoKHR info2{};
                info2.setPNext(&info1);
                info2.setHandle(sharings[i]);
                info2.setHandleType(vk::ExternalMemoryHandleTypeFlagBits::eD3D12Resource);

                vk::MemoryAllocateInfo info{};
                info.setPNext(&info2);
                info.setAllocationSize(reqs.size);
                info.setMemoryTypeIndex(index);
                memories[i] = device.allocateMemory(info, nullptr, dynamic);
                ASSERT_TRUE(memories[i]);
            }
            device.bindImageMemory(images[i], memories[i], 0, dynamic);
        }

        // cleanup
        for (auto i = 0u; i < count; ++i) {
            device.freeMemory(memories[i], nullptr, dynamic);
            device.destroyImage(images[i], nullptr, dynamic);
            CloseHandle(sharings[i]);
        }
    } catch (const vk::SystemError &e) {
        GTEST_FAIL() << e.what();
    } catch (const winrt::hresult_error &ex) {
        GTEST_FAIL() << winrt::to_string(ex.message());
    }
}