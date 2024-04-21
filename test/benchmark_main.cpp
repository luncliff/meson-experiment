// https://google.github.io/benchmark/user_guide.html
#include <benchmark/benchmark.h>

#include <chrono>
#include <coroutine>
#include <d3d11_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <ranges>
#include <winrt/windows.foundation.h>

#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "Dxva2.lib")

using namespace std::chrono_literals;

winrt::com_array<IMFActivate *> enum_device_sources(IMFAttributes *config) noexcept(false) {
    UINT32 num_devices = 0;
    IMFActivate **devices = nullptr;
    if (auto hr = MFEnumDeviceSources(config, &devices, &num_devices); FAILED(hr))
        winrt::throw_hresult(hr);
    return {static_cast<void *>(devices), num_devices, winrt::take_ownership_from_abi};
}

struct VideoCaptureFixture : public benchmark::Fixture {
    winrt::com_ptr<IMFActivate> device = nullptr;
    winrt::com_ptr<IMFMediaSource> source = nullptr;
    winrt::com_ptr<IMFSourceReader> reader = nullptr;

    void SetUp(benchmark::State &) {
        winrt::com_ptr<IMFAttributes> config = nullptr;
        winrt::check_hresult(MFCreateAttributes(config.put(), 1));
        config->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

        auto devices = enum_device_sources(config.get());
        for (auto item : std::ranges::views::reverse(devices)) {
            if (auto hr = item->QueryInterface(device.put()); FAILED(hr))
                continue;
            if (device != nullptr) // todo: parameterize the device selection
                break;
        }

        if (auto hr = device->ActivateObject(__uuidof(IMFMediaSource), source.put_void()); FAILED(hr))
            throw winrt::hresult_error{hr};

        if (auto hr = MFCreateSourceReaderFromMediaSource(source.get(), nullptr, reader.put()); FAILED(hr))
            throw winrt::hresult_error{hr};
        reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false);
        reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, true);
        // ...
    }
    void TearDown(benchmark::State &) {
        reader = nullptr;
        source = nullptr;
        device = nullptr;
    }
};

void read_samples(IMFSourceReader *reader, DWORD index, uint32_t count) {
    DWORD flags{};
    LONGLONG timestamp = 0; // unit 100-nanosecond
    for (uint32_t i = 0; i < count; ++i) {
        winrt::com_ptr<IMFSample> sample = nullptr;
        reader->ReadSample(index, 0, &index, &flags, &timestamp, sample.put());
    }
}

BENCHMARK_DEFINE_F(VideoCaptureFixture, ReadSamples)
(benchmark::State &state) {
    for (auto _ : state) {
        auto count = static_cast<uint32_t>(state.range(0));
        auto index = static_cast<DWORD>(state.range(1));
        if (index == 0)
            index = MF_SOURCE_READER_FIRST_VIDEO_STREAM;
        read_samples(reader.get(), index, count);
    }
}

void register_benchmarks() {
    BENCHMARK_REGISTER_F(VideoCaptureFixture, ReadSamples)
        ->Unit(benchmark::kSecond) //
        ->Args({64, 0})
        ->Args({128, 0})
        ->Args({256, 0})
        ->Args({512, 0});
}

int main(int argc, char *argv[]) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    winrt::check_hresult(MFStartup(MF_VERSION));
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
        return EXIT_FAILURE;
    register_benchmarks();
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return EXIT_SUCCESS;
}
