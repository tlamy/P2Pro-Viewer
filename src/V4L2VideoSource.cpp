#include "V4L2VideoSource.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <cstring>
#include <iostream>
#include "P2Pro.hpp" // For dprintf

#include <poll.h>
#include <errno.h>

V4L2VideoSource::V4L2VideoSource() {
}

V4L2VideoSource::~V4L2VideoSource() {
    close();
}

bool V4L2VideoSource::open(const std::string &device, int w, int h) {
    close();

    fd = ::open(device.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) {
        return false;
    }

    v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        close();
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        close();
        return false;
    }

    v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = w;
    fmt.fmt.pix.height = h;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        close();
        return false;
    }

    width = fmt.fmt.pix.width;
    height = fmt.fmt.pix.height;

    if (!init_mmap()) {
        close();
        return false;
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        close();
        return false;
    }

    return true;
}

bool V4L2VideoSource::init_mmap() {
    v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        return false;
    }

    for (uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            return false;
        }

        Buffer b;
        b.length = buf.length;
        b.start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (b.start == MAP_FAILED) {
            return false;
        }
        buffers.push_back(b);

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            return false;
        }
    }

    return true;
}

void V4L2VideoSource::close() {
    if (fd != -1) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);

        for (auto &b: buffers) {
            munmap(b.start, b.length);
        }
        buffers.clear();

        ::close(fd);
        fd = -1;
    }
}

bool V4L2VideoSource::getFrame(std::vector<uint8_t> &frameData) {
    if (fd == -1) return false;

    // Use poll to wait for data if it's not immediately available
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    // We wait up to 100ms for a frame
    int ret = poll(&pfd, 1, 100);
    if (ret <= 0) {
        return false;
    }

    v4l2_buffer buf;
    std::memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        return false;
    }

    if (buf.index >= buffers.size()) {
        ioctl(fd, VIDIOC_QBUF, &buf);
        return false;
    }

    frameData.assign((uint8_t *) buffers[buf.index].start, (uint8_t *) buffers[buf.index].start + buf.bytesused);

    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        return false;
    }

    return true;
}