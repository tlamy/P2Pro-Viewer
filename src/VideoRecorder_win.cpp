#define _CRT_SECURE_NO_WARNINGS
#include "VideoRecorder.hpp"
#include "P2Pro.hpp"
#include "ColorConversion.hpp"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <ctime>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")

struct VideoRecorderImpl {
    IMFSinkWriter* sinkWriter = nullptr;
    DWORD streamIndex = 0;
    LONGLONG frameDuration = 0;
    std::vector<uint8_t> bgrBuffer;
};

template <class T> void SafeRelease(T **ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

VideoRecorder::VideoRecorder() {
    MFStartup(MF_VERSION);
    impl = new VideoRecorderImpl();

    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &path))) {
        int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            std::vector<char> buf(len);
            WideCharToMultiByte(CP_UTF8, 0, path, -1, buf.data(), len, NULL, NULL);
            baseDir = buf.data();
        }
        CoTaskMemFree(path);
    } else if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &path))) {
        int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            std::vector<char> buf(len);
            WideCharToMultiByte(CP_UTF8, 0, path, -1, buf.data(), len, NULL, NULL);
            baseDir = buf.data();
        }
        CoTaskMemFree(path);
    }

    if (!baseDir.empty() && baseDir.back() != '\\' && baseDir.back() != '/') {
        baseDir += "\\";
    }

    dprintf("VideoRecorder::VideoRecorder() - Base directory: %s\n", baseDir.c_str());
}

VideoRecorder::~VideoRecorder() {
    stop();
    delete static_cast<VideoRecorderImpl*>(impl);
    MFShutdown();
}

std::string VideoRecorder::generateFilename() const {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << baseDir << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    return oss.str();
}

bool VideoRecorder::start(int w, int h, double f) {
    if (recording) return false;

    VideoRecorderImpl* v = static_cast<VideoRecorderImpl*>(impl);
    filename = generateFilename();
    width = w;
    height = h;
    fps = f;
    frame_count = 0;
    v->frameDuration = (LONGLONG)(10000000.0 / fps);

    HRESULT hr = S_OK;
    IMFAttributes* attributes = nullptr;
    IMFMediaType* outType = nullptr;
    IMFMediaType* inType = nullptr;

    // Create a wide-string filename
    int wlen = MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, NULL, 0);
    std::vector<WCHAR> wfilename(wlen);
    MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, wfilename.data(), wlen);

    hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) goto cleanup;
    hr = attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    if (FAILED(hr)) goto cleanup;

    hr = MFCreateSinkWriterFromURL(wfilename.data(), NULL, attributes, &v->sinkWriter);
    if (FAILED(hr)) {
        dprintf("VideoRecorder::start() - MFCreateSinkWriterFromURL failed hr=0x%08X\n", (unsigned)hr);
        goto cleanup;
    }

    // Output type (H.264)
    hr = MFCreateMediaType(&outType);
    if (FAILED(hr)) goto cleanup;
    hr = outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) goto cleanup;
    hr = outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (FAILED(hr)) goto cleanup;
    hr = outType->SetUINT32(MF_MT_AVG_BITRATE, 400000);
    if (FAILED(hr)) goto cleanup;
    hr = outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, width, height);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(outType, MF_MT_FRAME_RATE, (UINT32)fps, 1);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(outType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr)) goto cleanup;

    hr = v->sinkWriter->AddStream(outType, &v->streamIndex);
    if (FAILED(hr)) {
        dprintf("VideoRecorder::start() - AddStream failed hr=0x%08X\n", (unsigned)hr);
        goto cleanup;
    }

    // Input type (RGB32 - we'll convert RGB to BGR and pad with A)
    // Actually MFVideoFormat_RGB24 is also supported but often RGB32 is safer or faster
    hr = MFCreateMediaType(&inType);
    if (FAILED(hr)) goto cleanup;
    hr = inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) goto cleanup;
    hr = inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24); // We'll use RGB24 if possible
    if (FAILED(hr)) goto cleanup;
    hr = inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeSize(inType, MF_MT_FRAME_SIZE, width, height);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(inType, MF_MT_FRAME_RATE, (UINT32)fps, 1);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(inType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr)) goto cleanup;

    hr = v->sinkWriter->SetInputMediaType(v->streamIndex, inType, NULL);
    if (FAILED(hr)) {
        dprintf("VideoRecorder::start() - SetInputMediaType failed hr=0x%08X\n", (unsigned)hr);
        goto cleanup;
    }

    hr = v->sinkWriter->BeginWriting();
    if (FAILED(hr)) {
        dprintf("VideoRecorder::start() - BeginWriting failed hr=0x%08X\n", (unsigned)hr);
        goto cleanup;
    }

    v->bgrBuffer.resize(width * height * 3);
    recording = true;
    dprintf("VideoRecorder::start() - Recording started (Media Foundation): %s (%dx%d @ %.1f FPS)\n", filename.c_str(), width, height, fps);

cleanup:
    SafeRelease(&attributes);
    SafeRelease(&outType);
    SafeRelease(&inType);
    if (FAILED(hr)) {
        this->cleanup();
        return false;
    }
    return true;
}

void VideoRecorder::stop() {
    if (!recording) return;

    VideoRecorderImpl* v = static_cast<VideoRecorderImpl*>(impl);
    if (v->sinkWriter) {
        v->sinkWriter->Finalize();
    }
    cleanup();
    recording = false;
    dprintf("VideoRecorder::stop() - Recording stopped: %s\n", filename.c_str());
}

void VideoRecorder::cleanup() {
    VideoRecorderImpl* v = static_cast<VideoRecorderImpl*>(impl);
    SafeRelease(&v->sinkWriter);
}

void VideoRecorder::writeFrame(const std::vector<uint8_t>& rgb_data) {
    writeFrame(rgb_data.data());
}

void VideoRecorder::writeFrame(const uint8_t* rgb_data) {
    if (!recording || !rgb_data) return;

    VideoRecorderImpl* v = static_cast<VideoRecorderImpl*>(impl);

    IMFSample* sample = nullptr;
    IMFMediaBuffer* buffer = nullptr;
    HRESULT hr = S_OK;
    BYTE* pData = nullptr;

    // Convert RGB to BGR and flip (Media Foundation expects BGR for RGB24/RGB32,
    // and for some formats, bottom-up is default)
    ColorConversion::RGBtoBGRFlipped(rgb_data, v->bgrBuffer.data(), width, height);

    // Create sample
    hr = MFCreateMemoryBuffer((DWORD)v->bgrBuffer.size(), &buffer);
    if (FAILED(hr)) goto cleanup;

    hr = buffer->Lock(&pData, NULL, NULL);
    if (FAILED(hr)) goto cleanup;

    memcpy(pData, v->bgrBuffer.data(), v->bgrBuffer.size());
    hr = buffer->Unlock();
    if (FAILED(hr)) goto cleanup;

    hr = buffer->SetCurrentLength((DWORD)v->bgrBuffer.size());
    if (FAILED(hr)) goto cleanup;

    hr = MFCreateSample(&sample);
    if (FAILED(hr)) goto cleanup;

    hr = sample->AddBuffer(buffer);
    if (FAILED(hr)) goto cleanup;

    hr = sample->SetSampleTime(frame_count * v->frameDuration);
    if (FAILED(hr)) goto cleanup;

    hr = sample->SetSampleDuration(v->frameDuration);
    if (FAILED(hr)) goto cleanup;

    hr = v->sinkWriter->WriteSample(v->streamIndex, sample);
    if (SUCCEEDED(hr)) {
        frame_count++;
    } else {
        dprintf("VideoRecorder::writeFrame() - WriteSample failed hr=0x%08X\n", (unsigned)hr);
    }

cleanup:
    SafeRelease(&buffer);
    SafeRelease(&sample);
}
