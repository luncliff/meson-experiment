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
#include <mutex>
#include <ranges>
#include <thread>
#include <winrt/windows.foundation.h>
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "Dxva2.lib")

using namespace std::chrono_literals;

std::once_flag init_flag{};
void init() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    winrt::check_hresult(MFStartup(MF_VERSION));
}

winrt::com_array<IMFActivate *>
enum_device_sources(const GUID &source_type = MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID) noexcept(false) {
    winrt::com_ptr<IMFAttributes> config = nullptr;
    if (auto hr = MFCreateAttributes(config.put(), 1); FAILED(hr))
        winrt::throw_hresult(hr);
    config->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, source_type);

    UINT32 count = 0;
    IMFActivate **devices = nullptr;
    if (auto hr = MFEnumDeviceSources(config.get(), &devices, &count); FAILED(hr))
        winrt::throw_hresult(hr);
    return {static_cast<void *>(devices), count, winrt::take_ownership_from_abi};
}

winrt::com_ptr<IMFSourceReader> make_reader(IMFMediaSource *source,
                                            DWORD stream = MF_SOURCE_READER_FIRST_VIDEO_STREAM) {
    winrt::com_ptr<IMFSourceReader> reader = nullptr;
    if (auto hr = MFCreateSourceReaderFromMediaSource(source, nullptr, reader.put()); FAILED(hr))
        throw winrt::hresult_error{hr};
    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false);
    reader->SetStreamSelection(stream, true);
    return reader;
}

struct VideoCaptureFixture : public benchmark::Fixture {
    winrt::com_ptr<IMFActivate> device = nullptr;
    winrt::com_ptr<IMFMediaSource> source = nullptr;
    winrt::com_ptr<IMFSourceReader> reader = nullptr;
    winrt::com_ptr<IMFMediaType> media_type = nullptr;
    static constexpr DWORD stream = MF_SOURCE_READER_FIRST_VIDEO_STREAM;

    void SetUp(benchmark::State &state) {
        auto devices = enum_device_sources();
        auto device_index = static_cast<uint32_t>(state.range(0));
        if (auto hr = devices.at(device_index)->QueryInterface(device.put()); FAILED(hr))
            throw winrt::hresult_error{hr};
        if (auto hr = device->ActivateObject(__uuidof(IMFMediaSource), source.put_void()); FAILED(hr))
            throw winrt::hresult_error{hr};
        reader = make_reader(source.get());

        auto type_index = static_cast<uint32_t>(state.range(1));
        if (auto hr = reader->GetNativeMediaType(stream, type_index, media_type.put()); FAILED(hr))
            throw winrt::hresult_error{hr};
        if (auto hr = reader->SetCurrentMediaType(stream, nullptr, media_type.get()); FAILED(hr))
            throw winrt::hresult_error{hr};
    }
    void TearDown(benchmark::State &) {
        if (reader)
            reader->Flush(stream);
        reader = nullptr;
        if (device)
            device->ShutdownObject();
        device = nullptr;
    }

    std::string get_device_name() const noexcept(false) {
        UINT32 buflen = 0;
        auto hr = device->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, nullptr, 0, &buflen);
        auto buf = std::make_unique<WCHAR[]>(buflen + 1); // consider null termination
        hr = device->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, buf.get(), buflen + 1, nullptr);
        return winrt::to_string(std::wstring_view{buf.get(), buflen});
    }

    std::string get_format_name() const noexcept(false) {
        GUID subtype{};
        if (auto hr = media_type->GetGUID(MF_MT_SUBTYPE, &subtype); FAILED(hr))
            throw winrt::hresult_error{hr};
        if (IsEqualGUID(subtype, MFVideoFormat_NV12))
            return "NV12";
        if (IsEqualGUID(subtype, MFVideoFormat_MJPG))
            return "MJPG";
        if (IsEqualGUID(subtype, MFVideoFormat_YUY2))
            return "YUY2";
        if (IsEqualGUID(subtype, MFVideoFormat_I420))
            return "I420";
        if (IsEqualGUID(subtype, MFVideoFormat_IYUV))
            return "IYUV";
        return "Unknown";
    }

    std::tuple<UINT32, UINT32, float> get_format_size() const noexcept(false) {
        UINT32 width = 0;
        UINT32 height = 0;
        MFGetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE, &width, &height);
        UINT32 num = 0;
        UINT32 denom = 0;
        MFGetAttributeRatio(media_type.get(), MF_MT_FRAME_RATE, &num, &denom);
        float fps = static_cast<float>(num) / denom;
        return std::make_tuple(width, height, fps);
    }

    static void Register(benchmark::internal::Benchmark *benchmark) {
        std::call_once(init_flag, init);
        auto devices = enum_device_sources();
        for (auto device_index = 0; device_index < devices.size(); ++device_index) {
            auto device = devices.at(device_index);
            winrt::com_ptr<IMFMediaSource> source = nullptr;
            if (auto hr = device->ActivateObject(__uuidof(IMFMediaSource), source.put_void()); FAILED(hr))
                throw winrt::hresult_error{hr};
            winrt::com_ptr<IMFSourceReader> reader = make_reader(source.get());
            winrt::com_ptr<IMFMediaType> media_type = nullptr;
            for (DWORD type_index = 0; SUCCEEDED(reader->GetNativeMediaType(stream, type_index, media_type.put()));
                 ++type_index) {
                media_type = nullptr;
                benchmark->Args({device_index, type_index});
            }
            source->Shutdown();
        }
    }
};

void read_samples(IMFSourceReader *reader, DWORD stream, uint32_t count) {
    DWORD flags{};
    DWORD index = 0;
    LONGLONG timestamp = 0; // unit 100-nanosecond
    for (uint32_t i = 0; i < count; ++i) {
        winrt::com_ptr<IMFSample> sample = nullptr;
        reader->ReadSample(stream, 0, &index, &flags, &timestamp, sample.put());
        if (sample == nullptr)
            ++count;
    }
}

BENCHMARK_DEFINE_F(VideoCaptureFixture, ReadSamples)
(benchmark::State &state) {
    for (auto _ : state) {
        read_samples(reader.get(), stream, 64);
    }
    auto name = get_device_name();
    auto format = get_format_name();
    auto [width, height, fps] = get_format_size();
    state.SetLabel(std::format("{}, {}, {:d}, {:d}, {:.1f}", name, format, width, height, fps));
}

BENCHMARK_REGISTER_F(VideoCaptureFixture, ReadSamples)
    ->Apply(&VideoCaptureFixture::Register)
    ->Iterations(1)
    ->Unit(benchmark::kMillisecond);

int main(int argc, char *argv[]) {
    std::call_once(init_flag, init);
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
        return EXIT_FAILURE;
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return EXIT_SUCCESS;
}
