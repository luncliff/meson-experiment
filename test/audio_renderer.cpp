#include "audio_renderer.hpp"

#include <mferror.h>
#include <mfobjects.h>
#include <mmdeviceapi.h>

HRESULT mf_audio_renderer_t::QueryInterface(REFIID riid, void **ppv) {
    if (ppv == nullptr)
        return E_POINTER;

    if (IsEqualGUID(IID_IUnknown, riid))
        *ppv = static_cast<IUnknown *>(this);
    if (IsEqualGUID(IID_IMFAsyncResult, riid))
        *ppv = static_cast<IMFAsyncCallback *>(this);

    if (*ppv == nullptr)
        return E_NOINTERFACE;
    return S_OK;
}

ULONG mf_audio_renderer_t::AddRef() { return 1; }

ULONG mf_audio_renderer_t::Release() { return 1; }

HRESULT mf_audio_renderer_t::GetParameters(DWORD *, DWORD *) { return E_NOTIMPL; }

HRESULT mf_audio_renderer_t::Invoke(IMFAsyncResult *result) {
    winrt::com_ptr<IMFMediaEvent> event = nullptr;
    if (auto hr = session->EndGetEvent(result, event.put()); FAILED(hr))
        return hr;

    MediaEventType etype = MEUnknown;
    event->GetType(&etype);

    if (etype == MESessionClosed) {
        SetEvent(closed.get());
        return S_OK;
    }
    //if (etype == MESessionStarted) {
    //}
    //if (etype == MESessionPaused) {
    //}
    //if (etype == MESessionStopped) {
    //}
    if (etype == MESessionEnded) {
        PROPVARIANT vars{};
        if (loop)
            session->Start(nullptr, &vars);
    }

    return session->BeginGetEvent(this, nullptr);
}

mf_audio_renderer_t::mf_audio_renderer_t() noexcept(false) {
    winrt::check_hresult(MFCreateMediaSession(nullptr, session.put()));
    winrt::check_hresult(MFCreateTopology(topology.put()));
}

mf_audio_renderer_t::~mf_audio_renderer_t() noexcept {
    if (session) {
        session->Close();
        WaitForSingleObjectEx(closed.get(), INFINITE, false);
        session->Shutdown();
    }
}

HRESULT mf_audio_renderer_t::set_input(IMFMediaSource *source) noexcept {
    if (source == nullptr)
        return E_POINTER;
    presentation = nullptr;
    if (auto hr = source->CreatePresentationDescriptor(presentation.put()); FAILED(hr))
        return hr;
    descriptor = nullptr;
    DWORD stream_count = 0;
    presentation->GetStreamDescriptorCount(&stream_count);
    for (DWORD stream_index = 0; stream_index < stream_count; ++stream_index) {
        BOOL selected = false;
        winrt::com_ptr<IMFStreamDescriptor> candidate = nullptr;
        presentation->GetStreamDescriptorByIndex(stream_index, &selected, candidate.put());
        winrt::com_ptr<IMFMediaTypeHandler> handler = nullptr;
        candidate->GetMediaTypeHandler(handler.put());

        // found the audio stream descriptor?
        if (descriptor != nullptr)
            break;

        DWORD type_count = 0;
        handler->GetMediaTypeCount(&type_count);
        for (DWORD type_index = 0; type_index < type_count; ++type_index) {
            winrt::com_ptr<IMFMediaType> type = nullptr;
            handler->GetMediaTypeByIndex(type_index, type.put());
            GUID major{};
            type->GetMajorType(&major);
            if (IsEqualGUID(major, MFMediaType_Audio))
                descriptor = candidate;
        }
    }
    input = nullptr;
    MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, input.put());
    input->SetUnknown(MF_TOPONODE_SOURCE, source);
    input->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, presentation.get());
    input->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, descriptor.get());
    return topology->AddNode(input.get());
}

HRESULT mf_audio_renderer_t::set_output(IMFActivate *activate) noexcept {
    if (activate == nullptr)
        return E_POINTER;

    output = nullptr;
    MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, output.put());
    output->SetObject(activate);
    output->SetUINT32(MF_TOPONODE_STREAMID, 0);
    output->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, false);
    return topology->AddNode(output.get());
}

HRESULT mf_audio_renderer_t::connect() noexcept {
    if (input == nullptr || output == nullptr || topology == nullptr)
        return E_POINTER;
    if (auto hr = input->ConnectOutput(0, output.get(), 0); FAILED(hr))
        return hr;
    if (auto hr = session->SetTopology(0, topology.get()); FAILED(hr))
        return hr;
    return session->BeginGetEvent(this, nullptr);
}

void mf_audio_renderer_t::start() noexcept(false) {
    PROPVARIANT vars{};
    PropVariantInit(&vars);
    HRESULT hr = session->Start(nullptr, &vars);
    PropVariantClear(&vars);
    if (FAILED(hr))
        throw winrt::hresult_error{hr};
}

void mf_audio_renderer_t::stop() noexcept(false) {
    if (auto hr = session->Stop(); FAILED(hr))
        throw winrt::hresult_error{hr};
}

void mf_audio_renderer_t::pause() noexcept(false) {
    if (auto hr = session->Pause(); FAILED(hr))
        throw winrt::hresult_error{hr};
}
