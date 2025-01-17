#include <gtest/gtest.h>

#include <span>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

class TestEnvironment final : public ::testing::Environment {
    std::shared_ptr<spdlog::logger> m_logger = nullptr;

  public:
    explicit TestEnvironment(FILE *fout) noexcept {
        using sink0_type = spdlog::sinks::stdout_sink_base<spdlog::details::console_nullmutex>;
        auto sink0 = std::make_shared<sink0_type>(fout);
        m_logger = std::make_shared<spdlog::logger>("test", spdlog::sinks_init_list{sink0});
        spdlog::set_default_logger(m_logger);
        spdlog::set_pattern("%T.%e [%L] %8t %v");
#if defined(_DEBUG)
        spdlog::set_level(spdlog::level::level_enum::debug);
#endif
    }

  public:
    void SetUp() final {
        // ...
    }
    void TearDown() final {
        // ...
    }
};

#if defined(_WIN32)

/// @see https://learn.microsoft.com/en-us/cpp/c-language/using-wmain
int wmain(int argc, wchar_t *argv[], [[maybe_unused]] wchar_t *envp[]) {
    // WINRT_IMPL_CoInitializeEx
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new TestEnvironment{stdout});
    return RUN_ALL_TESTS();
}

#else

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new TestEnvironment{stdout});
    return RUN_ALL_TESTS();
}

#endif
