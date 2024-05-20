#include <gtest/gtest.h>

#include <spdlog/spdlog.h>

#include "experiment.hpp"

using namespace experiment;

struct NoopTest : public testing::Test {
    // ...
};

TEST_F(NoopTest, Noop) {
    // ...
    ASSERT_NE(get_build_version(), 0);
}

struct VulkanTest : public testing::Test {
    HMODULE mod = nullptr;

    void SetUp() final {
        mod = LoadLibraryW(L"vulkan-1.dll");
        // error?
    }
    void TearDown() final {
        if (mod == nullptr)
            return;
        EXPECT_TRUE(FreeLibrary(mod));
    }
};

TEST_F(VulkanTest, AvailableVersion) {
    if (mod == NULL)
        GTEST_SKIP();
    ASSERT_TRUE(check_vulkan_available(), true);

    uint32_t api_version = 0;
    uint32_t runtime_version = 0;
    bool ok = check_vulkan_runtime(api_version, runtime_version);
    ASSERT_NE(api_version, 0);
    if (!ok) {
        ASSERT_EQ(runtime_version, 0);
        return;
    }
    ASSERT_NE(runtime_version, 0);
}

/// @see https://learn.microsoft.com/en-us/cpp/c-language/using-wmain
int wmain(int argc, wchar_t *argv[], [[maybe_unused]] wchar_t *envp[]) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    ::testing::InitGoogleTest(&argc, argv);

    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("%T.%e [%L] %8t %v");

    auto runner = ::testing::UnitTest::GetInstance();
    return runner->Run();
}
