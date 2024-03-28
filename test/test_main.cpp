#include <gtest/gtest.h>

#include <cstdio>
#include <string_view>
#include <winrt/windows.foundation.h>

#include "sample.hpp"

struct NoopTest : public testing::Test {
    static constexpr std::string_view module_name = "windows-vulkan-experiment";
};

TEST_F(NoopTest, Noop) {
    // ...
    ASSERT_NE(module_name.length(), 0);
}

/// @see https://learn.microsoft.com/en-us/cpp/c-language/using-wmain
int wmain(int argc, wchar_t *argv[], [[maybe_unused]] wchar_t *envp[]) {
   winrt::init_apartment(winrt::apartment_type::multi_threaded);
   ::testing::InitGoogleTest(&argc, argv);
   auto runner = ::testing::UnitTest::GetInstance();
   return runner->Run();
}
