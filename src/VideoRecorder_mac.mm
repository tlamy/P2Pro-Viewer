#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>
#include "VideoRecorder.hpp"
#include "P2Pro.hpp" // For dprintf
#include <ctime>
#include <iomanip>
#include <sstream>

@interface MacOSVideoRecorder : NSObject
@property (nonatomic, strong) AVAssetWriter *writer;
@property (nonatomic, strong) AVAssetWriterInput *input;
@property (nonatomic, strong) AVAssetWriterInputPixelBufferAdaptor *adaptor;
@end

@implementation MacOSVideoRecorder
@end

struct VideoRecorderImpl {
    MacOSVideoRecorder* recorder;
};

VideoRecorder::VideoRecorder() {
    impl = new VideoRecorderImpl();
    VideoRecorderImpl* v = static_cast<VideoRecorderImpl*>(impl);
    v->recorder = [[MacOSVideoRecorder alloc] init];
}

VideoRecorder::~VideoRecorder() {
    stop();
    VideoRecorderImpl* v = static_cast<VideoRecorderImpl*>(impl);
    v->recorder = nil;
    delete v;
}

std::string VideoRecorder::generateFilename() const {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    return oss.str();
}

bool VideoRecorder::start(int w, int h, double f) {
    if (recording) return false;

    filename = generateFilename();
    width = w;
    height = h;
    fps = f;
    frame_count = 0;

    VideoRecorderImpl* v = static_cast<VideoRecorderImpl*>(impl);
    
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename.c_str()]];
    NSError *error = nil;
    v->recorder.writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&error];
    if (error || !v->recorder.writer) {
        dprintf("VideoRecorder::start() - Error creating AVAssetWriter: %s\n", [[error localizedDescription] UTF8String]);
        return false;
    }

    NSDictionary *videoSettings = @{
        AVVideoCodecKey: AVVideoCodecTypeH264,
        AVVideoWidthKey: @(width),
        AVVideoHeightKey: @(height),
    };

    v->recorder.input = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo outputSettings:videoSettings];
    v->recorder.input.expectsMediaDataInRealTime = YES;

    NSDictionary *pixelBufferAttributes = @{
        (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
        (id)kCVPixelBufferWidthKey: @(width),
        (id)kCVPixelBufferHeightKey: @(height)
    };

    v->recorder.adaptor = [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:v->recorder.input sourcePixelBufferAttributes:pixelBufferAttributes];

    if ([v->recorder.writer canAddInput:v->recorder.input]) {
        [v->recorder.writer addInput:v->recorder.input];
    } else {
        dprintf("VideoRecorder::start() - Could not add input to AVAssetWriter\n");
        return false;
    }

    if (![v->recorder.writer startWriting]) {
        dprintf("VideoRecorder::start() - Could not start writing: %s\n", [[v->recorder.writer.error localizedDescription] UTF8String]);
        return false;
    }

    [v->recorder.writer startSessionAtSourceTime:kCMTimeZero];

    dprintf("VideoRecorder::start() - Recording started (AVFoundation): %s (%dx%d @ %.1f FPS)\n", filename.c_str(), width, height, fps);
    recording = true;
    return true;
}

void VideoRecorder::stop() {
    if (!recording) return;

    VideoRecorderImpl* v = static_cast<VideoRecorderImpl*>(impl);
    [v->recorder.input markAsFinished];
    
    // We use a dispatch group or similar to wait for finishWriting, but for simplicity here:
    // actually finishWritingWithCompletionHandler is preferred.
    
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    [v->recorder.writer finishWritingWithCompletionHandler:^{
        dispatch_semaphore_signal(sema);
    }];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    v->recorder.writer = nil;
    v->recorder.input = nil;
    v->recorder.adaptor = nil;

    recording = false;
    dprintf("VideoRecorder::stop() - Recording stopped: %s\n", filename.c_str());
}

void VideoRecorder::cleanup() {
    // Handled in stop() and destructor
}

void VideoRecorder::writeFrame(const std::vector<uint8_t>& rgb_data) {
    if (!recording || rgb_data.empty()) return;

    VideoRecorderImpl* v = static_cast<VideoRecorderImpl*>(impl);
    if (!v->recorder.input.isReadyForMoreMediaData) return;

    CVPixelBufferRef pixelBuffer = NULL;
    CVReturn status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, v->recorder.adaptor.pixelBufferPool, &pixelBuffer);
    if (status != kCVReturnSuccess) {
        // Fallback to manual creation if pool is not ready
        NSDictionary *options = @{
            (id)kCVPixelBufferCGImageCompatibilityKey: @YES,
            (id)kCVPixelBufferCGBitmapContextCompatibilityKey: @YES
        };
        status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, kCVPixelFormatType_32BGRA, (__bridge CFDictionaryRef)options, &pixelBuffer);
    }

    if (status == kCVReturnSuccess && pixelBuffer) {
        CVPixelBufferLockBaseAddress(pixelBuffer, 0);
        uint8_t *baseAddress = (uint8_t*)CVPixelBufferGetBaseAddress(pixelBuffer);
        size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);

        for (int y = 0; y < height; ++y) {
            uint8_t *dst = baseAddress + y * bytesPerRow;
            const uint8_t *src = rgb_data.data() + y * width * 3;
            for (int x = 0; x < width; ++x) {
                dst[x*4 + 0] = src[x*3 + 2]; // B
                dst[x*4 + 1] = src[x*3 + 1]; // G
                dst[x*4 + 2] = src[x*3 + 0]; // R
                dst[x*4 + 3] = 255;          // A
            }
        }
        CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

        CMTime frameTime = CMTimeMake(frame_count, (int)fps);
        if (![v->recorder.adaptor appendPixelBuffer:pixelBuffer withPresentationTime:frameTime]) {
            dprintf("VideoRecorder::writeFrame() - Error appending pixel buffer: %s\n", [[v->recorder.writer.error localizedDescription] UTF8String]);
        }
        
        CVPixelBufferRelease(pixelBuffer);
        frame_count++;
    }
}
