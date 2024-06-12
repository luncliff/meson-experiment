#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOBITMAP
#include <gtest/gtest.h>

#include <chrono>
#include <coroutine>
#include <mutex>
#include <spdlog/spdlog.h>
#include <winrt/windows.foundation.h>

#include "audio_renderer.hpp"
#include <mferror.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <mmdeviceapi.h>

#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "Dxva2.lib")

winrt::com_array<IMFActivate *> enum_device_sources(const GUID &source_type) noexcept(false) {
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

winrt::com_ptr<IMFSourceReader> make_reader(IMFMediaSource *source, DWORD stream) {
    winrt::com_ptr<IMFSourceReader> reader = nullptr;
    if (auto hr = MFCreateSourceReaderFromMediaSource(source, nullptr, reader.put()); FAILED(hr))
        throw winrt::hresult_error{hr};
    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false);
    reader->SetStreamSelection(stream, true);
    return reader;
}

struct MFAudioSourceTest : public testing::Test {
    static constexpr DWORD stream = MF_SOURCE_READER_FIRST_AUDIO_STREAM;
    winrt::com_ptr<IMFActivate> device = nullptr;
    winrt::com_ptr<IMFMediaSource> source = nullptr;
    winrt::com_ptr<IMFSourceReader> reader = nullptr;
    winrt::com_ptr<IMFMediaType> media_type = nullptr;

    void SetUp() {
        winrt::check_hresult(MFStartup(MFSTARTUP_FULL));
        auto devices = enum_device_sources(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
        if (devices.empty())
            GTEST_SKIP();
        if (auto hr = devices.at(0)->QueryInterface(device.put()); FAILED(hr))
            throw winrt::hresult_error{hr};
        if (auto hr = device->ActivateObject(__uuidof(IMFMediaSource), source.put_void()); FAILED(hr))
            throw winrt::hresult_error{hr};
        reader = make_reader(source.get(), stream);
    }
    void TearDown() {
        if (reader)
            reader->Flush(stream);
        reader = nullptr;
        if (device)
            device->ShutdownObject();
        device = nullptr;
        MFShutdown();
    }
};

TEST_F(MFAudioSourceTest, Noop) {
    // ...
    ASSERT_NE(reader, nullptr);
}

winrt::com_ptr<IMMDevice> get_audio_endpoint() {
    winrt::com_ptr<IMMDeviceEnumerator> enumerator = nullptr;
    if (auto hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   enumerator.put_void());
        FAILED(hr))
        throw winrt::hresult_error{hr};
    winrt::com_ptr<IMMDeviceCollection> collection = nullptr;
    if (auto hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.put()); FAILED(hr))
        throw winrt::hresult_error{hr};

    UINT count = 0;
    collection->GetCount(&count);
    if (count == 0)
        return nullptr;

    winrt::com_ptr<IMMDevice> device = nullptr;
    if (auto hr = collection->Item(0, device.put()); FAILED(hr))
        throw winrt::hresult_error{hr};
    return device;
}

struct MFAudioRendererTest : public testing::Test {
    static constexpr DWORD stream = MF_SOURCE_READER_FIRST_AUDIO_STREAM;
    winrt::com_ptr<IMMDevice> device = nullptr;
    std::wstring device_id{};

    void SetUp() {
        winrt::check_hresult(MFStartup(MFSTARTUP_FULL));
        device = get_audio_endpoint();
        ASSERT_NE(device, nullptr);
        if (LPWSTR id = nullptr; SUCCEEDED(device->GetId(&id))) {
            device_id = id;
            CoTaskMemFree(id);
        }
        // ...
    }
    void TearDown() {
        device = nullptr;
        MFShutdown();
    }

    HRESULT create_renderer(IMFMediaSink **output) noexcept {
        winrt::com_ptr<IMFAttributes> attrs = nullptr;
        if (auto hr = MFCreateAttributes(attrs.put(), 2); FAILED(hr))
            return hr;
        attrs->SetString(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID, device_id.c_str());
        attrs->SetGUID(MF_AUDIO_RENDERER_ATTRIBUTE_SESSION_ID, GUID_NULL);
        return MFCreateAudioRenderer(attrs.get(), output);
    }

    HRESULT create_renderer(IMFActivate **output) noexcept {
        winrt::com_ptr<IMFActivate> activate = nullptr;
        if (auto hr = MFCreateAudioRendererActivate(activate.put()); FAILED(hr))
            return hr;
        activate->SetString(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID, device_id.c_str());
        // activate->SetGUID(MF_AUDIO_RENDERER_ATTRIBUTE_SESSION_ID, GUID_NULL);
        activate->SetUINT32(MF_AUDIO_RENDERER_ATTRIBUTE_FLAGS, MF_AUDIO_RENDERER_ATTRIBUTE_FLAGS_CROSSPROCESS);
        return activate->QueryInterface(output);
    }
};

TEST_F(MFAudioRendererTest, CreateMediaSink) {
    winrt::com_ptr<IMFMediaSink> sink = nullptr;
    ASSERT_EQ(create_renderer(sink.put()), S_OK);
}

TEST_F(MFAudioRendererTest, CreateMFActivate) {
    winrt::com_ptr<IMFActivate> activate = nullptr;
    ASSERT_EQ(create_renderer(activate.put()), S_OK);
}

TEST_F(MFAudioRendererTest, AudioWithReader) {
    GTEST_SKIP();
    winrt::com_ptr<IMFMediaSource> source = nullptr;
    {
        winrt::com_ptr<IMFSourceResolver> resolver = nullptr;
        ASSERT_EQ(MFCreateSourceResolver(resolver.put()), S_OK);

        winrt::com_ptr<IUnknown> unknown = nullptr;
        auto asset = L"C:/Users/luncl/Downloads/4_1_StoryChaboon.mp3";
        MF_OBJECT_TYPE type = MF_OBJECT_INVALID;
        ASSERT_EQ(resolver->CreateObjectFromURL(asset, MF_RESOLUTION_MEDIASOURCE, nullptr, &type, unknown.put()), S_OK);
        ASSERT_EQ(unknown->QueryInterface(source.put()), S_OK);
    }

    winrt::com_ptr<IMFSourceReader> reader = nullptr;
    ASSERT_EQ(MFCreateSourceReaderFromMediaSource(source.get(), nullptr, reader.put()), S_OK);

    winrt::com_ptr<IMFMediaSink> sink = nullptr;
    ASSERT_EQ(create_renderer(sink.put()), S_OK);

    winrt::com_ptr<IMFStreamSink> stream_sink = nullptr;
    ASSERT_EQ(sink->GetStreamSinkByIndex(0, stream_sink.put()), S_OK);

    winrt::com_ptr<IMFMediaTypeHandler> handler = nullptr;
    ASSERT_EQ(stream_sink->GetMediaTypeHandler(handler.put()), S_OK);

    DWORD count = 0;
    handler->GetMediaTypeCount(&count);

    winrt::com_ptr<IMFMediaType> sink_type = nullptr;
    for (DWORD i = 0; i < count; ++i) {
        handler->GetMediaTypeByIndex(i, sink_type.put());
        if (auto hr = handler->IsMediaTypeSupported(sink_type.get(), nullptr); FAILED(hr))
            sink_type = nullptr;
    }
    ASSERT_NE(sink_type, nullptr);
    ASSERT_EQ(reader->SetCurrentMediaType(stream, nullptr, sink_type.get()), S_OK);

    winrt::com_ptr<IMFSinkWriter> writer = nullptr;
    ASSERT_EQ(MFCreateSinkWriterFromMediaSink(sink.get(), nullptr, writer.put()), S_OK);
    ASSERT_EQ(writer->SetInputMediaType(0, sink_type.get(), nullptr), S_OK);

    writer->BeginWriting();

    std::chrono::milliseconds consumed{};
    bool finished = false;
    while (finished == false) {
        DWORD actual = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        winrt::com_ptr<IMFSample> sample = nullptr;
        if (auto hr = reader->ReadSample(stream, 0, &actual, &flags, &timestamp, sample.put()); FAILED(hr))
            ASSERT_EQ(hr, S_OK);

        if (flags & MF_SOURCE_READERF_ERROR)
            ASSERT_TRUE(sample);

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            finished = true;
            continue;
        }

        if (flags & MF_SOURCE_READERF_STREAMTICK)
            writer->SendStreamTick(0, timestamp);

        if (sample) {
            if (auto hr = writer->WriteSample(0, sample.get()); FAILED(hr))
                ASSERT_EQ(hr, S_OK);
            using namespace std::chrono;
            auto elapsed = duration_cast<milliseconds>(nanoseconds{timestamp * 100});
            if (auto diff = elapsed - consumed; diff > diff.zero()) {
                SleepEx((diff * 0.9f).count(), true);
                consumed = elapsed;
            }
        }
    }

    source->Shutdown();
    writer->Finalize();
}

TEST_F(MFAudioRendererTest, AudioWithSessionTopology) {
    GTEST_SKIP();
    winrt::com_ptr<IMFMediaSource> source = nullptr;
    {
        winrt::com_ptr<IMFSourceResolver> resolver = nullptr;
        ASSERT_EQ(MFCreateSourceResolver(resolver.put()), S_OK);

        winrt::com_ptr<IUnknown> unknown = nullptr;
        auto asset = L"C:/Users/USER/Downloads/maldita.wav";
        MF_OBJECT_TYPE type = MF_OBJECT_INVALID;
        ASSERT_EQ(resolver->CreateObjectFromURL(asset, MF_RESOLUTION_MEDIASOURCE, nullptr, &type, unknown.put()), S_OK);
        ASSERT_EQ(unknown->QueryInterface(source.put()), S_OK);
    }

    winrt::com_ptr<IMFActivate> activate = nullptr;
    ASSERT_EQ(create_renderer(activate.put()), S_OK);
    auto renderer = std::make_unique<mf_audio_renderer_t>();
    renderer->loop = true;

    ASSERT_EQ(renderer->set_input(source.get()), S_OK);
    ASSERT_EQ(renderer->set_output(activate.get()), S_OK);
    ASSERT_EQ(renderer->connect(), S_OK);

    ASSERT_NO_THROW(renderer->start());
    // SleepEx(2'000, true);

    // ASSERT_NO_THROW(renderer->pause());
    // SleepEx(2'000, true);

    // ASSERT_NO_THROW(renderer->start());
    SleepEx(20'000, true);

    ASSERT_NO_THROW(renderer->stop());
}
