#include "dragdrop.h"

#include <objidl.h>
#include <oleidl.h>

#include <cstring>
#include <string>

namespace {

class Data final : public IDataObject {
public:
    explicit Data(std::wstring text) : text_(std::move(text)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return E_POINTER;
        if (id == IID_IUnknown || id == IID_IDataObject) {
            *out = (IDataObject*)this;
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG left = --refs_;
        if (!left) delete this;
        return left;
    }

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* format, STGMEDIUM* medium) override {
        if (!format || !medium) return E_POINTER;
        if (format->cfFormat != CF_UNICODETEXT) return DV_E_FORMATETC;
        const size_t bytes = (text_.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory) return E_OUTOFMEMORY;
        if (void* target = ::GlobalLock(memory)) {
            std::memcpy(target, text_.c_str(), bytes);
            ::GlobalUnlock(memory);
        }
        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = memory;
        medium->pUnkForRelease = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* format) override {
        if (!format) return E_POINTER;
        return format->cfFormat == CF_UNICODETEXT ? S_OK : DV_E_FORMATETC;
    }

    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction,
                                           IEnumFORMATETC** out) override {
        if (direction != DATADIR_GET || !out) return E_NOTIMPL;
        FORMATETC format{};
        format.cfFormat = CF_UNICODETEXT;
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1;
        format.tymed = TYMED_HGLOBAL;
        return ::OleRegEnumFormatEtc(DATADIR_GET, 1, &format, out);
    }

    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }

private:
    ~Data() = default;
    ULONG refs_ = 1;
    std::wstring text_;
};

class Source final : public IDropSource {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return E_POINTER;
        if (id == IID_IUnknown || id == IID_IDropSource) {
            *out = (IDropSource*)this;
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG left = --refs_;
        if (!left) delete this;
        return left;
    }

    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
        if (escape) return DRAGDROP_S_CANCEL;
        if (!(keys & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }

private:
    ~Source() = default;
    ULONG refs_ = 1;
};

}  // namespace

void BeginTextDrag(HWND owner, const std::wstring& text) {
    if (text.empty()) return;
    auto* object = new Data(text);
    auto* source = new Source();
    DWORD effect = DROPEFFECT_COPY;
    ::DoDragDrop(object, source, DROPEFFECT_COPY | DROPEFFECT_MOVE, &effect);
    source->Release();
    object->Release();
    ::SetFocus(owner);
}
