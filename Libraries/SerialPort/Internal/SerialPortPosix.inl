// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Foundation/StringPath.h"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace SC
{
namespace detail
{
static bool mapBaudToNative(uint32_t baudRate, speed_t& outBaud)
{
    switch (baudRate)
    {
    case 0: outBaud = B0; return true;
    case 50: outBaud = B50; return true;
    case 75: outBaud = B75; return true;
    case 110: outBaud = B110; return true;
    case 134: outBaud = B134; return true;
    case 150: outBaud = B150; return true;
    case 200: outBaud = B200; return true;
    case 300: outBaud = B300; return true;
    case 600: outBaud = B600; return true;
    case 1200: outBaud = B1200; return true;
    case 1800: outBaud = B1800; return true;
    case 2400: outBaud = B2400; return true;
    case 4800: outBaud = B4800; return true;
    case 9600: outBaud = B9600; return true;
    case 19200: outBaud = B19200; return true;
    case 38400: outBaud = B38400; return true;
#ifdef B57600
    case 57600: outBaud = B57600; return true;
#endif
#ifdef B115200
    case 115200: outBaud = B115200; return true;
#endif
#ifdef B230400
    case 230400: outBaud = B230400; return true;
#endif
#ifdef B460800
    case 460800: outBaud = B460800; return true;
#endif
#ifdef B500000
    case 500000: outBaud = B500000; return true;
#endif
#ifdef B576000
    case 576000: outBaud = B576000; return true;
#endif
#ifdef B921600
    case 921600: outBaud = B921600; return true;
#endif
#ifdef B1000000
    case 1000000: outBaud = B1000000; return true;
#endif
#ifdef B1152000
    case 1152000: outBaud = B1152000; return true;
#endif
#ifdef B1500000
    case 1500000: outBaud = B1500000; return true;
#endif
#ifdef B2000000
    case 2000000: outBaud = B2000000; return true;
#endif
#ifdef B2500000
    case 2500000: outBaud = B2500000; return true;
#endif
#ifdef B3000000
    case 3000000: outBaud = B3000000; return true;
#endif
#ifdef B3500000
    case 3500000: outBaud = B3500000; return true;
#endif
#ifdef B4000000
    case 4000000: outBaud = B4000000; return true;
#endif
    default: return false;
    }
}

static bool mapNativeToBaud(speed_t nativeBaud, uint32_t& outBaud)
{
    struct BaudMapping
    {
        speed_t  nativeValue;
        uint32_t logicalValue;
    };
    static const BaudMapping mappings[] = {
        {B0, 0},
        {B50, 50},
        {B75, 75},
        {B110, 110},
        {B134, 134},
        {B150, 150},
        {B200, 200},
        {B300, 300},
        {B600, 600},
        {B1200, 1200},
        {B1800, 1800},
        {B2400, 2400},
        {B4800, 4800},
        {B9600, 9600},
        {B19200, 19200},
        {B38400, 38400},
#ifdef B57600
        {B57600, 57600},
#endif
#ifdef B115200
        {B115200, 115200},
#endif
#ifdef B230400
        {B230400, 230400},
#endif
#ifdef B460800
        {B460800, 460800},
#endif
#ifdef B500000
        {B500000, 500000},
#endif
#ifdef B576000
        {B576000, 576000},
#endif
#ifdef B921600
        {B921600, 921600},
#endif
#ifdef B1000000
        {B1000000, 1000000},
#endif
#ifdef B1152000
        {B1152000, 1152000},
#endif
#ifdef B1500000
        {B1500000, 1500000},
#endif
#ifdef B2000000
        {B2000000, 2000000},
#endif
#ifdef B2500000
        {B2500000, 2500000},
#endif
#ifdef B3000000
        {B3000000, 3000000},
#endif
#ifdef B3500000
        {B3500000, 3500000},
#endif
#ifdef B4000000
        {B4000000, 4000000},
#endif
    };

    for (const auto& mapping : mappings)
    {
        if (mapping.nativeValue == nativeBaud)
        {
            outBaud = mapping.logicalValue;
            return true;
        }
    }
    return false;
}

Result openSerialHandle(StringSpan path, const SerialOpenOptions& options, FileDescriptor::Handle& outHandle)
{
    SC_TRY_MSG(path.getEncoding() != StringEncoding::Utf16,
               "SerialDescriptor::open - Posix supports only UTF8 and ASCII");
    StringPath nullTerminatedPath;
    SC_TRY_MSG(nullTerminatedPath.assign(path), "SerialDescriptor::open - Invalid path");
    const char* nativePath = nullTerminatedPath.view().getNullTerminatedNative();
    SC_TRY_MSG(nativePath[0] == '/', "SerialDescriptor::open - Path must be absolute");

    int flags = O_RDWR | O_NOCTTY;
    if (not options.blocking)
    {
        flags |= O_NONBLOCK;
    }
#ifdef O_CLOEXEC
    if (not options.inheritable)
    {
        flags |= O_CLOEXEC;
    }
#endif
#ifdef O_EXCL
    if (options.exclusive)
    {
        flags |= O_EXCL;
    }
#endif

    int serialFd;
    do
    {
        serialFd = ::open(nativePath, flags, 0);
    } while (serialFd == -1 and errno == EINTR);
    SC_TRY_MSG(serialFd != -1, "SerialDescriptor::open - open failed");

    if (not options.inheritable)
    {
        int descriptorFlags;
        do
        {
            descriptorFlags = ::fcntl(serialFd, F_GETFD);
        } while (descriptorFlags == -1 and errno == EINTR);
        if (descriptorFlags == -1)
        {
            (void)::close(serialFd);
            return Result::Error("SerialDescriptor::open - fcntl(F_GETFD) failed");
        }
        if ((descriptorFlags & FD_CLOEXEC) == 0)
        {
            int setFlags;
            do
            {
                setFlags = ::fcntl(serialFd, F_SETFD, descriptorFlags | FD_CLOEXEC);
            } while (setFlags == -1 and errno == EINTR);
            if (setFlags == -1)
            {
                (void)::close(serialFd);
                return Result::Error("SerialDescriptor::open - fcntl(F_SETFD) failed");
            }
        }
    }

    outHandle = serialFd;
    return Result(true);
}

Result setSerialSettings(FileDescriptor::Handle handle, const SerialSettings& settings)
{
    struct termios tty;

    int result;
    do
    {
        result = ::tcgetattr(handle, &tty);
    } while (result == -1 and errno == EINTR);
    SC_TRY_MSG(result == 0, "SerialDescriptor::setSettings - tcgetattr failed");

#if defined(CFMAKE_RAW)
    ::cfmakeraw(&tty);
#else
    tty.c_iflag &= ~static_cast<tcflag_t>(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_oflag &= ~static_cast<tcflag_t>(OPOST);
    tty.c_lflag &= ~static_cast<tcflag_t>(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_cflag &= ~static_cast<tcflag_t>(CSIZE | PARENB);
    tty.c_cflag |= CS8;
#endif

    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~static_cast<tcflag_t>(CSIZE);
    switch (settings.dataBits)
    {
    case SerialSettings::DataBits::Bits5: tty.c_cflag |= CS5; break;
    case SerialSettings::DataBits::Bits6: tty.c_cflag |= CS6; break;
    case SerialSettings::DataBits::Bits7: tty.c_cflag |= CS7; break;
    case SerialSettings::DataBits::Bits8: tty.c_cflag |= CS8; break;
    }

    tty.c_cflag &= ~static_cast<tcflag_t>(PARENB | PARODD);
    switch (settings.parity)
    {
    case SerialSettings::Parity::None: break;
    case SerialSettings::Parity::Odd:
        tty.c_cflag |= PARENB;
        tty.c_cflag |= PARODD;
        break;
    case SerialSettings::Parity::Even: tty.c_cflag |= PARENB; break;
    }

    tty.c_cflag &= ~static_cast<tcflag_t>(CSTOPB);
    if (settings.stopBits == SerialSettings::StopBits::Two)
    {
        tty.c_cflag |= CSTOPB;
    }

    tty.c_iflag &= ~static_cast<tcflag_t>(IXON | IXOFF | IXANY);
#ifdef CRTSCTS
    tty.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
#endif
    switch (settings.flowControl)
    {
    case SerialSettings::FlowControl::None: break;
    case SerialSettings::FlowControl::Software:
        tty.c_iflag |= IXON;
        tty.c_iflag |= IXOFF;
        break;
    case SerialSettings::FlowControl::Hardware:
#ifdef CRTSCTS
        tty.c_cflag |= CRTSCTS;
#else
        return Result::Error("SerialDescriptor::setSettings - Hardware flow control not supported");
#endif
        break;
    }

    speed_t nativeBaud = B9600;
    SC_TRY_MSG(mapBaudToNative(settings.baudRate, nativeBaud), "SerialDescriptor::setSettings - Unsupported baudRate");
    SC_TRY_MSG(::cfsetispeed(&tty, nativeBaud) == 0, "SerialDescriptor::setSettings - cfsetispeed failed");
    SC_TRY_MSG(::cfsetospeed(&tty, nativeBaud) == 0, "SerialDescriptor::setSettings - cfsetospeed failed");

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    do
    {
        result = ::tcsetattr(handle, TCSANOW, &tty);
    } while (result == -1 and errno == EINTR);
    SC_TRY_MSG(result == 0, "SerialDescriptor::setSettings - tcsetattr failed");

    return Result(true);
}

Result getSerialSettings(FileDescriptor::Handle handle, SerialSettings& settings)
{
    struct termios tty;
    int            result;
    do
    {
        result = ::tcgetattr(handle, &tty);
    } while (result == -1 and errno == EINTR);
    SC_TRY_MSG(result == 0, "SerialDescriptor::getSettings - tcgetattr failed");

    const speed_t inputBaud = ::cfgetispeed(&tty);
    SC_TRY_MSG(mapNativeToBaud(inputBaud, settings.baudRate), "SerialDescriptor::getSettings - Unsupported baudRate");

    switch (tty.c_cflag & CSIZE)
    {
    case CS5: settings.dataBits = SerialSettings::DataBits::Bits5; break;
    case CS6: settings.dataBits = SerialSettings::DataBits::Bits6; break;
    case CS7: settings.dataBits = SerialSettings::DataBits::Bits7; break;
    case CS8: settings.dataBits = SerialSettings::DataBits::Bits8; break;
    default: return Result::Error("SerialDescriptor::getSettings - Unsupported dataBits");
    }

    if ((tty.c_cflag & PARENB) == 0)
    {
        settings.parity = SerialSettings::Parity::None;
    }
    else if ((tty.c_cflag & PARODD) != 0)
    {
        settings.parity = SerialSettings::Parity::Odd;
    }
    else
    {
        settings.parity = SerialSettings::Parity::Even;
    }

    settings.stopBits = (tty.c_cflag & CSTOPB) != 0 ? SerialSettings::StopBits::Two : SerialSettings::StopBits::One;

    if ((tty.c_iflag & (IXON | IXOFF)) != 0)
    {
        settings.flowControl = SerialSettings::FlowControl::Software;
    }
    else
    {
#ifdef CRTSCTS
        settings.flowControl =
            (tty.c_cflag & CRTSCTS) != 0 ? SerialSettings::FlowControl::Hardware : SerialSettings::FlowControl::None;
#else
        settings.flowControl = SerialSettings::FlowControl::None;
#endif
    }
    return Result(true);
}
} // namespace detail
} // namespace SC
