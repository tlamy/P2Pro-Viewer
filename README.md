# InfiRay P2 Pro Viewer

Original work (in Python) by [LeoDJ](https://github.com/LeoDJ/P2Pro-Viewer) and [Pinoerkel](https://github.com/Pinoerkel/P2Pro-Viewer).

I basically took the reverse-engineered protocol and let AI build the rest.

:warning: **[WIP]**  
See [below](#roadmap) for the original roadmap.

This project aims to be an open-source image viewer and API for the InfiRay P2 Pro thermal camera module.

The communication protocol was reverse-engineered, to avoid needing to include the proprietary precompiled InfiRay libraries.


# Compiling
## MacOS
```
brew install sdl2 sdl2_ttf
```
## Linux (Debian/Ubuntu)
```
sudo apt install libsdl2-dev libusb-1.0-0-dev libavcodec-dev libavformat-dev libswscale-dev libavutil-dev
```

## Windows
It is recommended to use [vcpkg](https://github.com/microsoft/vcpkg) in manifest mode to manage dependencies.
1. Ensure you have `vcpkg` installed on your system.
2. Set the `VCPKG_ROOT` environment variable to your `vcpkg` installation directory.
3. When configuring the project with CMake, `vcpkg` will automatically download and install the required libraries (`sdl2`, `sdl2-ttf`, `libusb`).

Video recording on Windows uses native Media Foundation APIs instead of FFmpeg.

## Notices
### Windows
For sending vendor control transfers to the UVC camera in addition to opening the camera as a normal UVC camera, `libusb` needs to be able to access the device. On Windows, this requires installing a **Filter Driver** (not a full replacement) so both the video stream and control commands can work simultaneously.

**To fix "Device not found" or "Permission denied" errors on Windows:**
1.  Download and run [Zadig](https://zadig.akeo.ie/).
2.  Go to **Options** > **List All Devices**.
3.  Select **"USB Camera (Interface 0)"** (VID: `0BDA`, PID: `5830`).
4.  In the driver selection box (next to the green arrow), select **`libusb-win32`** or **`WinUSB`**.
5.  **CRITICAL**: Click the **small down arrow** next to the big button and select **"Install Filter Driver"**.
    *   *Do **not** click "Replace Driver" directly, as that will disable the camera for all other Windows applications.*
6.  Restart the P2ProViewer.

Also, the camera video stream needs to be opened first before sending commands to it, otherwise the call to `libusb` will just hang for whatever reason. This is handled automatically by the `WindowsAdapter` in this project.

### Linux

To use the P2Pro camera on Linux without root privileges, you need to set up udev rules.

1. Copy the provided udev rules file to your system:
   ```bash
   sudo cp 60-p2pro.rules /etc/udev/rules.d/
   ```
2. Reload the udev rules:
   ```bash
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```
3. Ensure your user is in the `video` group:
   ```bash
   sudo usermod -aG video $USER
   ```
   (You may need to log out and back in for this to take effect).

The C++ viewer uses native APIs (IOKit on macOS, libusb on Linux) for control commands and (AVFoundation/V4L2) for the video stream. Video recording is handled by native APIs (AVAssetWriter) on macOS and FFmpeg on Linux. By default, it will search for
the camera on `/dev/video*` devices (Linux) or via AVFoundation (macOS).

## Where to buy
The cheapest vendor in Germany appears to be [Peargear](https://www.pergear.de/products/infiray-p2-pro?ref=067mg).  
Pergear also has [an international shop](https://www.pergear.com/products/infiray-p2-pro?ref=067mg) for other countries, but I'm not sure if they're the cheapest there.

## Additional Resources
- [Review and teardown video by mikeselectricstuff](https://www.youtube.com/watch?v=YMQeXq1ujn0)
- [Extensive review thread by Fraser](https://www.eevblog.com/forum/thermal-imaging/review-infiray-p2-pro-thermal-camera-dongle-for-android-mobile-phones/)
- [General discussion thread with some interesting resources](https://www.eevblog.com/forum/thermal-imaging/infiray-and-their-p2-pro-discussion/)


## (Original) Roadmap
- [ ] USB Vendor Commands
    - [x] Read/Write commands
        - [x] "standard" cmd
        - [x] "long" cmd
        - [x] wait for camera ready
    - [x] Pseudo color 
    - [ ] NUC shutter control (auto/manual/trigger)
    - [ ] High/low temperature range
    - [ ] Other parameters (emissivity, distance, etc)
    - [ ] Switch to actual raw sensor readings?
    - [ ] Recalibrate lens?
    - [ ] Remaining (less relevant) commands
- [ ] Recording
    - [ ] Still image
        - [ ] JPEG and radiometry data in one file
            - [ ] Standardized format? R-JPEG?
        - [ ] Metadata (rotation, camera settings, location?, etc)
    - [ ] Video
        - [x] MKV file with radiometry data as second lossless video track
        - [x] Audio
        - [ ] Metadata
        - [ ] Rotation (on-the-fly possible? :D)
        - [ ] Render overlays into video (scale, min/max/center temps, etc)
        - [ ] Timelapse?
        - [ ] Min/Max/Center temperature in subtitles? :D
        - [ ] Standardized video format? (don't know any)
    - [ ] Find / build tool to analyze recordings later (or export to other formats)
    - [ ] Virtual webcam output with temperature scale overlays?
- [ ] GUI
    - [x] Display video stream
    - [ ] Overlays
        - [ ] Temperature scale
        - [x] Min/max/center temperature
        - [ ] Cursor hover temperature
    - [ ] Palette selection
    - [ ] Shutter control
    - [ ] Gain selection (camera temperature range)
    - [ ] Parameters (emissivity, distance, etc)
    - [x] Recording controls (take picture, start/stop video, recording indicator and time)
    - [ ] Image rotation / flip
    - [ ] Manually set min/max range (probably need to apply own pseudo color from raw temperature data)
    - [ ] Own analysis controls (points, lines, rectangles, etc)
    - [ ] Log measurements to CSV
    - [ ] Plot measurements
    - [ ] ...
- [ ] Documentation
    - [ ] How to use
    - [ ] USB vendor command protocol
    - [ ] My video format (if I don't find a more standardized one)
    - [ ] P2Pro Android app JPEG file format, that has radiometry data embedded
- Very long-term plans:
    - Small device with a socket for the P2 Pro to convert it into a hand-held device
    - Android App
