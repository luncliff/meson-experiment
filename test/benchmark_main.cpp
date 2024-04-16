// https://google.github.io/benchmark/user_guide.html
#include <benchmark/benchmark.h>

#include <chrono>
#include <thread>
#include <winrt/windows.foundation.h>

using namespace std::chrono_literals;

struct NoopFixture : public benchmark::Fixture {
    void SetUp(benchmark::State &) {
        // ...
    }
    void TearDown(benchmark::State &) {
        // ...
    }
};

BENCHMARK_F(NoopFixture, Noop)(benchmark::State &state) {
    // state.SkipWithError("noop");
    for (auto _ : state)
        std::this_thread::sleep_for(2s);
}

int main(int argc, char *argv[]) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
        return EXIT_FAILURE;
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return EXIT_SUCCESS;
}
