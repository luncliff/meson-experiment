#include <gtest/gtest.h>

#include <cstdio>
#include <string_view>

#include "experiment.hpp"

using namespace experiment;

struct NoopTest : public testing::Test {
    // ...
};

TEST_F(NoopTest, Noop) {
    // ...
    ASSERT_NE(get_build_version(), 0);
}

/// @see https://learn.microsoft.com/en-us/cpp/c-language/using-wmain
int wmain(int argc, wchar_t *argv[], [[maybe_unused]] wchar_t *envp[]) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    ::testing::InitGoogleTest(&argc, argv);
    auto runner = ::testing::UnitTest::GetInstance();
    return runner->Run();
}
