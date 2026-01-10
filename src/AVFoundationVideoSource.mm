#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#include "AVFoundationVideoSource.hpp"
#include "P2Pro.hpp" // For dprintf
#include <mutex>
#include <vector>
#include <iostream>

@interface P2ProCaptureDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate> {
    std::vector<uint8_t> latestFrame;
    std::mutex frameMutex;
    BOOL hasNewFrame;
}
@property (nonatomic, strong) AVCaptureSession *session;
@property (nonatomic, strong) AVCaptureDeviceInput *input;
@property (nonatomic, strong) AVCaptureVideoDataOutput *output;
@property (nonatomic, assign) BOOL isOpened;

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
- (bool)getLatestFrame:(std::vector<uint8_t>&)frameData;
@end

@implementation P2ProCaptureDelegate

- (instancetype)init {
    self = [super init];
    if (self) {
        hasNewFrame = NO;
        _isOpened = NO;
    }
    return self;
}

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection {
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer) return;

    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
    
    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    void *baseAddress = CVPixelBufferGetBaseAddress(imageBuffer);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(imageBuffer);
    
    // We expect YUYV (kCVPixelFormatType_422YpCbCr8) which is 2 bytes per pixel
    size_t expectedBytesPerRow = width * 2;
    
    std::lock_guard<std::mutex> lock(frameMutex);
    latestFrame.resize(width * height * 2);
    
    if (bytesPerRow == expectedBytesPerRow) {
        memcpy(latestFrame.data(), baseAddress, width * height * 2);
    } else {
        for (size_t y = 0; y < height; ++y) {
            memcpy(latestFrame.data() + y * expectedBytesPerRow, (uint8_t*)baseAddress + y * bytesPerRow, expectedBytesPerRow);
        }
    }
    
    hasNewFrame = YES;
    
    CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
}

- (bool)getLatestFrame:(std::vector<uint8_t>&)frameData {
    std::lock_guard<std::mutex> lock(frameMutex);
    if (!hasNewFrame) return false;
    frameData = latestFrame;
    hasNewFrame = NO; 
    return true;
}

@end

AVFoundationVideoSource::AVFoundationVideoSource() {
    impl = (__bridge_retained void*)[[P2ProCaptureDelegate alloc] init];
}

AVFoundationVideoSource::~AVFoundationVideoSource() {
    close();
    P2ProCaptureDelegate* delegate = (__bridge_transfer P2ProCaptureDelegate*)impl;
    delegate = nil;
}

std::vector<std::string> AVFoundationVideoSource::listDevices() {
    std::vector<std::string> deviceNames;
    AVCaptureDeviceDiscoverySession *discoverySession = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeExternal] 
                                                                                                          mediaType:AVMediaTypeVideo 
                                                                                                           position:AVCaptureDevicePositionUnspecified];
    for (AVCaptureDevice *device in discoverySession.devices) {
        deviceNames.push_back([[device localizedName] UTF8String]);
    }
    return deviceNames;
}

bool AVFoundationVideoSource::open(int index, int width, int height, int fps) {
    AVCaptureDeviceDiscoverySession *discoverySession = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeExternal] 
                                                                                                          mediaType:AVMediaTypeVideo 
                                                                                                           position:AVCaptureDevicePositionUnspecified];
    NSArray *devices = discoverySession.devices;
    if (index < 0 || index >= (int)[devices count]) return false;
    AVCaptureDevice *device = [devices objectAtIndex:index];
    
    return openByName([[device localizedName] UTF8String], width, height, fps);
}

bool AVFoundationVideoSource::openByName(const std::string& name, int width, int height, int fps) {
    close();
    
    P2ProCaptureDelegate* delegate = (__bridge P2ProCaptureDelegate*)impl;
    
    AVCaptureDevice *targetDevice = nil;
    AVCaptureDeviceDiscoverySession *discoverySession = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeExternal] 
                                                                                                          mediaType:AVMediaTypeVideo 
                                                                                                           position:AVCaptureDevicePositionUnspecified];
    for (AVCaptureDevice *device in discoverySession.devices) {
        if ([[device localizedName] UTF8String] == name) {
            targetDevice = device;
            break;
        }
    }
    
    if (!targetDevice) return false;
    
    NSError *error = nil;
    delegate.input = [AVCaptureDeviceInput deviceInputWithDevice:targetDevice error:&error];
    if (error || !delegate.input) {
        dprintf("AVFoundationVideoSource::openByName() - Error creating input: %s\n", [[error localizedDescription] UTF8String]);
        return false;
    }
    
    delegate.session = [[AVCaptureSession alloc] init];
    [delegate.session beginConfiguration];
    
    if ([delegate.session canAddInput:delegate.input]) {
        [delegate.session addInput:delegate.input];
    } else {
        dprintf("AVFoundationVideoSource::openByName() - Could not add input to session.\n");
        return false;
    }
    
    delegate.output = [[AVCaptureVideoDataOutput alloc] init];
    // Request YUYV format
    [delegate.output setVideoSettings:@{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_422YpCbCr8_yuvs)}];
    [delegate.output setAlwaysDiscardsLateVideoFrames:YES];
    
    dispatch_queue_t queue = dispatch_queue_create("cameraQueue", NULL);
    [delegate.output setSampleBufferDelegate:delegate queue:queue];
    
    if ([delegate.session canAddOutput:delegate.output]) {
        [delegate.session addOutput:delegate.output];
    } else {
        dprintf("AVFoundationVideoSource::openByName() - Could not add output to session.\n");
        return false;
    }
    
    // Set format (resolution and FPS)
    AVCaptureDeviceFormat *bestFormat = nil;
    for (AVCaptureDeviceFormat *format in [targetDevice formats]) {
        CMVideoFormatDescriptionRef desc = format.formatDescription;
        CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(desc);
        if (dims.width == width && dims.height == height) {
            bestFormat = format;
            // Check for FPS
            for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges) {
                if (range.maxFrameRate >= fps && range.minFrameRate <= fps) {
                    bestFormat = format;
                    break;
                }
            }
        }
    }
    
    if (bestFormat) {
        if ([targetDevice lockForConfiguration:&error]) {
            targetDevice.activeFormat = bestFormat;
            targetDevice.activeVideoMinFrameDuration = CMTimeMake(1, fps);
            targetDevice.activeVideoMaxFrameDuration = CMTimeMake(1, fps);
            [targetDevice unlockForConfiguration];
            dprintf("AVFoundationVideoSource::openByName() - Set format to %dx%d @ %d FPS\n", width, height, fps);
        }
    } else {
        dprintf("AVFoundationVideoSource::openByName() - Warning: Could not find exact format %dx%d @ %d FPS. Using default.\n", width, height, fps);
    }
    
    [delegate.session commitConfiguration];
    [delegate.session startRunning];
    
    delegate.isOpened = YES;
    return true;
}

void AVFoundationVideoSource::close() {
    P2ProCaptureDelegate* delegate = (__bridge P2ProCaptureDelegate*)impl;
    if (delegate.session) {
        [delegate.session stopRunning];
        delegate.session = nil;
        delegate.input = nil;
        delegate.output = nil;
    }
    delegate.isOpened = NO;
}

bool AVFoundationVideoSource::isOpened() const {
    P2ProCaptureDelegate* delegate = (__bridge P2ProCaptureDelegate*)impl;
    return delegate.isOpened;
}

bool AVFoundationVideoSource::getFrame(std::vector<uint8_t>& frameData) {
    P2ProCaptureDelegate* delegate = (__bridge P2ProCaptureDelegate*)impl;
    return [delegate getLatestFrame:frameData];
}
