#pragma once
#include <mfapi.h>
#include <mfidl.h>
#include <winrt/windows.foundation.h>

class mf_audio_renderer_t final : public IMFAsyncCallback {
    winrt::handle closed{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    winrt::com_ptr<IMFMediaSession> session = nullptr;
    winrt::com_ptr<IMFTopology> topology = nullptr;
    winrt::com_ptr<IMFTopologyNode> input = nullptr;
    winrt::com_ptr<IMFPresentationDescriptor> presentation = nullptr;
    winrt::com_ptr<IMFStreamDescriptor> descriptor = nullptr;
    winrt::com_ptr<IMFTopologyNode> output = nullptr;

  public:
    mf_audio_renderer_t() noexcept(false);
    ~mf_audio_renderer_t() noexcept;

    HRESULT set_input(IMFMediaSource *source) noexcept;
    HRESULT set_output(IMFActivate *activate) noexcept;
    HRESULT connect() noexcept;

    void start() noexcept(false);
    void stop() noexcept(false);
    void pause() noexcept(false);
    void resume() noexcept(false);

  public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE GetParameters(DWORD *, DWORD *);
    HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult *result);
};
