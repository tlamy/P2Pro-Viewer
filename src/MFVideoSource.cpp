#include "MFVideoSource.hpp"
#include "P2Pro.hpp"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <mutex>
#include <iostream>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")

struct MFVideoSourceImpl {
    IMFSourceReader* sourceReader = nullptr;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> latestFrame;
    std::mutex frameMutex;
    bool opened = false;
};

MFVideoSource::MFVideoSource() {
    MFStartup(MF_VERSION);
    impl = new MFVideoSourceImpl();
}

MFVideoSource::~MFVideoSource() {
    close();
    delete static_cast<MFVideoSourceImpl*>(impl);
    MFShutdown();
}

// Format a GUID's FourCC bytes as a human-readable string (e.g. "YUY2", "NV12").
// MF video format GUIDs encode the FourCC in the low 4 bytes of Data1.
static std::string fourcCStr(const GUID& g) {
    char s[5];
    s[0] = (char)((g.Data1 >>  0) & 0xFF);
    s[1] = (char)((g.Data1 >>  8) & 0xFF);
    s[2] = (char)((g.Data1 >> 16) & 0xFF);
    s[3] = (char)((g.Data1 >> 24) & 0xFF);
    s[4] = '\0';
    for (int k = 0; k < 4; k++)
        if (s[k] < 32 || s[k] > 126) s[k] = '?';
    return s;
}

bool MFVideoSource::open(int width, int height) {
    MFVideoSourceImpl* v = static_cast<MFVideoSourceImpl*>(impl);
    if (v->opened) return true;

    IMFAttributes* attributes = nullptr;
    HRESULT hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) {
        dprintf("MFVideoSource: MFCreateAttributes failed hr=0x%08X\n", (unsigned)hr);
        return false;
    }

    hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) { attributes->Release(); return false; }

    IMFActivate** devices = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(attributes, &devices, &count);
    attributes->Release();
    if (FAILED(hr)) {
        dprintf("MFVideoSource: MFEnumDeviceSources failed hr=0x%08X\n", (unsigned)hr);
        return false;
    }

    dprintf("MFVideoSource: %u video capture device(s) found\n", count);

    bool found = false;
    for (UINT32 i = 0; i < count; ++i) {
        WCHAR* friendlyName = nullptr;
        UINT32 nameLen = 0;
        devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &friendlyName, &nameLen);
        dprintf("MFVideoSource: device[%u] = \"%ls\"\n", i, friendlyName ? friendlyName : L"(unknown)");

        IMFMediaSource* source = nullptr;
        hr = devices[i]->ActivateObject(IID_PPV_ARGS(&source));
        if (FAILED(hr)) {
            dprintf("MFVideoSource:   ActivateObject failed hr=0x%08X\n", (unsigned)hr);
            CoTaskMemFree(friendlyName);
            continue;
        }

        // Do NOT set MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING: it causes MF to
        // normalize chrominance (U/V) bytes toward 128, which corrupts both the
        // pseudo-color image (flat grey/green tint) and the thermal Y16 data
        // (MSB forced to 0x80 → reads ~240 °C instead of room temperature).
        // We request the native YUY2 type directly, so no conversion pipeline is needed.
        hr = MFCreateSourceReaderFromMediaSource(source, nullptr, &v->sourceReader);
        source->Release();

        if (FAILED(hr)) {
            dprintf("MFVideoSource:   MFCreateSourceReaderFromMediaSource failed hr=0x%08X\n", (unsigned)hr);
            CoTaskMemFree(friendlyName);
            continue;
        }

        // Log every native media type so we can see exactly what the camera offers
        DWORD mediaTypeIndex = 0;
        bool formatFound = false;
        while (true) {
            IMFMediaType* mediaType = nullptr;
            hr = v->sourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, mediaTypeIndex, &mediaType);
            if (FAILED(hr)) break;

            GUID subtype = {};
            mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
            UINT32 w = 0, h = 0;
            MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &w, &h);

            bool isTarget = (subtype == MFVideoFormat_YUY2 && (int)w == width && (int)h == height);
            dprintf("MFVideoSource:   type[%u] %ux%u %s%s\n",
                    mediaTypeIndex, w, h, fourcCStr(subtype).c_str(),
                    isTarget ? " <-- match" : "");

            if (isTarget) {
                hr = v->sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mediaType);
                if (SUCCEEDED(hr)) {
                    formatFound = true;
                    v->width = width;
                    v->height = height;
                } else {
                    dprintf("MFVideoSource:   SetCurrentMediaType failed hr=0x%08X\n", (unsigned)hr);
                }
            }
            mediaType->Release();
            if (formatFound) break;
            mediaTypeIndex++;
        }

        if (formatFound) {
            dprintf("MFVideoSource:   opened successfully\n");
            v->opened = true;
            found = true;
        } else {
            dprintf("MFVideoSource:   no YUY2 %dx%d format found on this device\n", width, height);
            v->sourceReader->Release();
            v->sourceReader = nullptr;
        }

        CoTaskMemFree(friendlyName);
        if (found) break;
    }

    for (UINT32 i = 0; i < count; ++i) devices[i]->Release();
    CoTaskMemFree(devices);

    return found;
}

void MFVideoSource::close() {
    MFVideoSourceImpl* v = static_cast<MFVideoSourceImpl*>(impl);
    if (v->sourceReader) {
        v->sourceReader->Release();
        v->sourceReader = nullptr;
    }
    v->opened = false;
}

bool MFVideoSource::isOpened() const {
    return static_cast<MFVideoSourceImpl*>(impl)->opened;
}

bool MFVideoSource::getFrame(std::vector<uint8_t>& frameData) {
    MFVideoSourceImpl* v = static_cast<MFVideoSourceImpl*>(impl);
    if (!v->opened) return false;

    DWORD streamIndex = 0, flags = 0;
    LONGLONG timestamp = 0;
    IMFSample* sample = nullptr;
    HRESULT hr = S_OK;

    // At stream start MF often delivers stream-tick events (null sample, S_OK) before
    // the first real frame arrives.  Retry a few times to skip past them.
    for (int attempt = 0; attempt < 10; ++attempt) {
        hr = v->sourceReader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
            &streamIndex, &flags, &timestamp, &sample);

        if (FAILED(hr)) {
            dprintf("MFVideoSource: ReadSample failed hr=0x%08X flags=0x%08X\n",
                    (unsigned)hr, flags);
            return false;
        }
        if (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM)) {
            dprintf("MFVideoSource: ReadSample stream terminated (flags=0x%08X)\n", flags);
            return false;
        }
        if (sample) break;  // Got a real video frame
        // Otherwise a stream tick or format-change event — retry
    }

    if (!sample) {
        dprintf("MFVideoSource: ReadSample yielded no sample after retries (flags=0x%08X)\n", flags);
        return false;
    }

    IMFMediaBuffer* buffer = nullptr;
    hr = sample->GetBufferByIndex(0, &buffer);
    if (SUCCEEDED(hr)) {
        BYTE* data = nullptr;
        DWORD maxLength, currentLength;
        hr = buffer->Lock(&data, &maxLength, &currentLength);
        if (SUCCEEDED(hr)) {
            frameData.assign(data, data + currentLength);
            buffer->Unlock();
        }
        buffer->Release();
    }
    sample->Release();

    return SUCCEEDED(hr);
}
