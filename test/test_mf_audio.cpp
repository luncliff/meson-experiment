#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOBITMAP
#include <gtest/gtest.h>

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
#include <mmdeviceapi.h>
#include <mutex>
#include <ranges>
#include <spdlog/spdlog.h>
#include <thread>
#include <winrt/windows.foundation.h>
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
        activate->SetGUID(MF_AUDIO_RENDERER_ATTRIBUTE_SESSION_ID, GUID_NULL);
        return activate->QueryInterface(output);
    }
};

TEST_F(MFAudioRendererTest, CreateRenderer) {
    winrt::com_ptr<IMFMediaSink> sink = nullptr;
    winrt::com_ptr<IMFAttributes> attrs = nullptr;
    MFCreateAttributes(attrs.put(), 2);
    ASSERT_EQ(attrs->SetString(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID, device_id.c_str()), S_OK);
    ASSERT_EQ(attrs->SetGUID(MF_AUDIO_RENDERER_ATTRIBUTE_SESSION_ID, GUID_NULL), S_OK);
    ASSERT_EQ(MFCreateAudioRenderer(attrs.get(), sink.put()), S_OK);
    ASSERT_NE(sink, nullptr);
}

TEST_F(MFAudioRendererTest, CreateMFActivate) {
    winrt::com_ptr<IMFActivate> activate = nullptr;
    ASSERT_EQ(MFCreateAudioRendererActivate(activate.put()), S_OK);
    ASSERT_EQ(activate->SetString(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID, device_id.c_str()), S_OK);
    ASSERT_EQ(activate->SetGUID(MF_AUDIO_RENDERER_ATTRIBUTE_SESSION_ID, GUID_NULL), S_OK);
    // ActivateObject ...
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

struct TestAsyncCallback final : public IMFAsyncCallback {
    winrt::com_ptr<IMFMediaSession> session = nullptr;
    // winrt::com_ptr<IMFMediaEventGenerator> events = nullptr;
    std::atomic_bool started = false;
    std::atomic_bool ended = false;
    std::atomic_bool closed = false;

  public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) {
        if (ppv == nullptr)
            return E_POINTER;

        if (IsEqualGUID(IID_IUnknown, riid)) {
            *ppv = static_cast<IUnknown *>(this);
            return S_OK;
        }
        if (IsEqualGUID(IID_IMFAsyncResult, riid)) {
            *ppv = static_cast<IMFAsyncCallback *>(this);
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() { return 1; }

    ULONG STDMETHODCALLTYPE Release() { return 1; }

    HRESULT STDMETHODCALLTYPE GetParameters(DWORD *, DWORD *) { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult *result) {
        winrt::com_ptr<IMFMediaEvent> event = nullptr;
        if (auto hr = session->EndGetEvent(result, event.put()); FAILED(hr))
            return hr;

        MediaEventType etype = MEUnknown;
        event->GetType(&etype);
        if (etype == MESessionClosed) {
            closed = true;
            return S_OK;
        }
        if (etype == MESessionStarted) {
            started = true;
        }
        if (etype == MESessionEnded) {
            ended = true;
        }

        return session->BeginGetEvent(this, nullptr);
    }
};

TEST_F(MFAudioRendererTest, AudioWithSessionTopology) {
    GTEST_SKIP();
    winrt::com_ptr<IMFTopology> topology = nullptr;
    ASSERT_EQ(MFCreateTopology(topology.put()), S_OK);

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

    winrt::com_ptr<IMFPresentationDescriptor> presentation = nullptr;
    ASSERT_EQ(source->CreatePresentationDescriptor(presentation.put()), S_OK);

    winrt::com_ptr<IMFTopologyNode> input = nullptr;

    DWORD stream_count = 0;
    presentation->GetStreamDescriptorCount(&stream_count);
    for (DWORD stream_index = 0; stream_index < stream_count; ++stream_index) {
        BOOL selected = false;
        winrt::com_ptr<IMFStreamDescriptor> descriptor = nullptr;
        ASSERT_EQ(presentation->GetStreamDescriptorByIndex(stream_index, &selected, descriptor.put()), S_OK);
        if (selected == false)
            continue;

        winrt::com_ptr<IMFMediaTypeHandler> handler = nullptr;
        descriptor->GetMediaTypeHandler(handler.put());

        GUID major{};
        handler->GetMajorType(&major);
        ASSERT_TRUE(IsEqualGUID(major, MFMediaType_Audio));

        DWORD type_count = 0;
        handler->GetMediaTypeCount(&type_count);
        winrt::com_ptr<IMFMediaType> sink_type = nullptr;
        for (DWORD type_index = 0; type_index < type_count; ++type_index) {
            handler->GetMediaTypeByIndex(type_index, sink_type.put());
            if (auto hr = handler->IsMediaTypeSupported(sink_type.get(), nullptr); FAILED(hr))
                sink_type = nullptr;
        }
        ASSERT_NE(sink_type, nullptr);

        MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, input.put());
        input->SetUnknown(MF_TOPONODE_SOURCE, source.get());
        input->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, presentation.get());
        input->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, descriptor.get());
        ASSERT_EQ(topology->AddNode(input.get()), S_OK);
    }

    winrt::com_ptr<IMFActivate> renderer = nullptr;
    ASSERT_EQ(create_renderer(renderer.put()), S_OK);

    // winrt::com_ptr<IMFAudioStreamVolume> volume = nullptr;
    // if (auto hr = MFGetService(renderer.get(), MR_POLICY_VOLUME_SERVICE, //
    //                            __uuidof(IMFAudioStreamVolume), volume.put_void());
    //     FAILED(hr))
    //     ASSERT_EQ(hr, S_OK);

    winrt::com_ptr<IMFTopologyNode> output = nullptr;
    {
        MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, output.put());
        output->SetObject(renderer.get());
        output->SetUINT32(MF_TOPONODE_STREAMID, 0); // use the default
        output->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, false);
        ASSERT_EQ(topology->AddNode(output.get()), S_OK);
    }

    ASSERT_EQ(input->ConnectOutput(0, output.get(), 0), S_OK);

    winrt::com_ptr<IMFMediaSession> session = nullptr;
    ASSERT_EQ(MFCreateMediaSession(nullptr, session.put()), S_OK);

    auto handler = std::make_unique<TestAsyncCallback>();
    handler->session = session;
    ASSERT_EQ(session->BeginGetEvent(handler.get(), nullptr), S_OK);

    winrt::com_ptr<IMFSimpleAudioVolume> volume = nullptr;
    if (SUCCEEDED(
            MFGetService(session.get(), MR_POLICY_VOLUME_SERVICE, __uuidof(IMFSimpleAudioVolume), volume.put_void()))) {
        volume->SetMute(false);
        volume->SetMasterVolume(0.8f);
    }

    if (auto hr = session->SetTopology(MFSESSION_SETTOPOLOGY_CLEAR_CURRENT, topology.get()); FAILED(hr))
        ASSERT_EQ(hr, S_OK);

    {
        PROPVARIANT vars{};
        PropVariantInit(&vars);
        ASSERT_EQ(session->Start(nullptr, &vars), S_OK);
        PropVariantClear(&vars);
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) != false) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // while (handler->closed == false) {
    //     // ...
    //     // winrt::com_ptr<IMFClock> clock = nullptr;
    //     // session->GetClock(clock.put());
    //     SleepEx(1000, true);
    // }

    ASSERT_EQ(session->Stop(), S_OK);

    session->Close();
    source->Shutdown();
    session->Shutdown();
    // renderer->DetachObject();
}
