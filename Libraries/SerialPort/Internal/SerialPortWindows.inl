// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../../Common/IGrowableBufferStringPath.h"

#include <string.h>
#include <wchar.h>

namespace SC
{
namespace detail
{
static bool isUnprefixedComPath(const wchar_t* path)
{
    if (path == nullptr)
        return false;
    if (not((path[0] == L'C' or path[0] == L'c') and (path[1] == L'O' or path[1] == L'o') and
            (path[2] == L'M' or path[2] == L'm')))
    {
        return false;
    }
    const wchar_t* digits = path + 3;
    if (*digits == 0)
    {
        return false;
    }
    while (*digits)
    {
        if (*digits < L'0' or *digits > L'9')
        {
            return false;
        }
        ++digits;
    }
    return true;
}

static ResultSerialPort setTimeouts(HANDLE serialHandle, bool blocking)
{
    COMMTIMEOUTS timeouts;
    ::ZeroMemory(&timeouts, sizeof(timeouts));
    if (blocking)
    {
        timeouts.ReadIntervalTimeout         = 50;
        timeouts.ReadTotalTimeoutMultiplier  = 10;
        timeouts.ReadTotalTimeoutConstant    = 50;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant   = 0;
    }
    else
    {
        timeouts.ReadIntervalTimeout         = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier  = 0;
        timeouts.ReadTotalTimeoutConstant    = 0;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant   = 0;
    }
    if (::SetCommTimeouts(serialHandle, &timeouts) == FALSE)
        return ResultSerialPort(SerialPortError::OpenFailed, SerialPortErrorDetail::WindowsSetTimeouts,
                                static_cast<uint32_t>(::GetLastError()));
    return ResultSerialPort(true);
}

ResultSerialPort openSerialHandle(StringSpan path, const SerialOpenOptions& options, FileDescriptor::Handle& outHandle)
{
    StringPath nullTerminatedPath;
    if (not nullTerminatedPath.assign(path))
        return ResultSerialPort(SerialPortError::InvalidPath);
    const wchar_t* nativePath = nullTerminatedPath.view().getNullTerminatedNative();

    SECURITY_ATTRIBUTES securityAttributes  = {};
    securityAttributes.nLength              = sizeof(securityAttributes);
    securityAttributes.bInheritHandle       = options.inheritable ? TRUE : FALSE;
    securityAttributes.lpSecurityDescriptor = nullptr;

    const DWORD desiredAccess = GENERIC_READ | GENERIC_WRITE;
    const DWORD shareMode     = options.exclusive ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
    const DWORD creationMode  = OPEN_EXISTING;
    const DWORD flagsAndAttrs = options.blocking ? FILE_ATTRIBUTE_NORMAL : FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED;

    HANDLE serialHandle =
        ::CreateFileW(nativePath, desiredAccess, shareMode, &securityAttributes, creationMode, flagsAndAttrs, nullptr);
    if (serialHandle == INVALID_HANDLE_VALUE and isUnprefixedComPath(nativePath))
    {
        wchar_t prefixedPath[StringPath::StorageCapacity] = {};

        const size_t pathLen = ::wcslen(nativePath);
        if (pathLen + 4 > StringPath::MaxPath)
            return ResultSerialPort(SerialPortError::PathTooLong);
        prefixedPath[0] = L'\\';
        prefixedPath[1] = L'\\';
        prefixedPath[2] = L'.';
        prefixedPath[3] = L'\\';
        ::memcpy(prefixedPath + 4, nativePath, (pathLen + 1) * sizeof(wchar_t));

        serialHandle = ::CreateFileW(prefixedPath, desiredAccess, shareMode, &securityAttributes, creationMode,
                                     flagsAndAttrs, nullptr);
    }
    if (serialHandle == INVALID_HANDLE_VALUE)
        return ResultSerialPort(SerialPortError::OpenFailed, SerialPortErrorDetail::WindowsCreateFile,
                                static_cast<uint32_t>(::GetLastError()));
    SC_TRY(setTimeouts(serialHandle, options.blocking));
    outHandle = serialHandle;
    return ResultSerialPort(true);
}

ResultSerialPort setSerialSettings(FileDescriptor::Handle handle, const SerialSettings& settings)
{
    DCB dcb;
    ::ZeroMemory(&dcb, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (::GetCommState(handle, &dcb) == FALSE)
        return ResultSerialPort(SerialPortError::ReadSettingsFailed, SerialPortErrorDetail::WindowsReadSettings,
                                static_cast<uint32_t>(::GetLastError()));

    dcb.fBinary  = TRUE;
    dcb.BaudRate = settings.baudRate;

    switch (settings.dataBits)
    {
    case SerialSettings::DataBits::Bits5: dcb.ByteSize = 5; break;
    case SerialSettings::DataBits::Bits6: dcb.ByteSize = 6; break;
    case SerialSettings::DataBits::Bits7: dcb.ByteSize = 7; break;
    case SerialSettings::DataBits::Bits8: dcb.ByteSize = 8; break;
    }

    switch (settings.parity)
    {
    case SerialSettings::Parity::None: dcb.Parity = NOPARITY; break;
    case SerialSettings::Parity::Odd: dcb.Parity = ODDPARITY; break;
    case SerialSettings::Parity::Even: dcb.Parity = EVENPARITY; break;
    }

    switch (settings.stopBits)
    {
    case SerialSettings::StopBits::One: dcb.StopBits = ONESTOPBIT; break;
    case SerialSettings::StopBits::Two: dcb.StopBits = TWOSTOPBITS; break;
    }

    dcb.fOutxCtsFlow    = FALSE;
    dcb.fOutxDsrFlow    = FALSE;
    dcb.fDtrControl     = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX           = FALSE;
    dcb.fInX            = FALSE;
    dcb.fRtsControl     = RTS_CONTROL_DISABLE;

    switch (settings.flowControl)
    {
    case SerialSettings::FlowControl::None: break;
    case SerialSettings::FlowControl::Software:
        dcb.fOutX = TRUE;
        dcb.fInX  = TRUE;
        break;
    case SerialSettings::FlowControl::Hardware:
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl  = RTS_CONTROL_HANDSHAKE;
        break;
    }

    if (::SetCommState(handle, &dcb) == FALSE)
        return ResultSerialPort(SerialPortError::SetSettingsFailed, SerialPortErrorDetail::WindowsSetSettings,
                                static_cast<uint32_t>(::GetLastError()));
    return ResultSerialPort(true);
}

ResultSerialPort getSerialSettings(FileDescriptor::Handle handle, SerialSettings& settings)
{
    DCB dcb;
    ::ZeroMemory(&dcb, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (::GetCommState(handle, &dcb) == FALSE)
        return ResultSerialPort(SerialPortError::ReadSettingsFailed, SerialPortErrorDetail::WindowsReadSettings,
                                static_cast<uint32_t>(::GetLastError()));

    settings.baudRate = dcb.BaudRate;
    switch (dcb.ByteSize)
    {
    case 5: settings.dataBits = SerialSettings::DataBits::Bits5; break;
    case 6: settings.dataBits = SerialSettings::DataBits::Bits6; break;
    case 7: settings.dataBits = SerialSettings::DataBits::Bits7; break;
    case 8: settings.dataBits = SerialSettings::DataBits::Bits8; break;
    default: return ResultSerialPort(SerialPortError::UnsupportedDataBits);
    }

    switch (dcb.Parity)
    {
    case NOPARITY: settings.parity = SerialSettings::Parity::None; break;
    case ODDPARITY: settings.parity = SerialSettings::Parity::Odd; break;
    case EVENPARITY: settings.parity = SerialSettings::Parity::Even; break;
    default: return ResultSerialPort(SerialPortError::UnsupportedParity);
    }

    switch (dcb.StopBits)
    {
    case ONESTOPBIT: settings.stopBits = SerialSettings::StopBits::One; break;
    case TWOSTOPBITS: settings.stopBits = SerialSettings::StopBits::Two; break;
    default: return ResultSerialPort(SerialPortError::UnsupportedStopBits);
    }

    if (dcb.fOutX or dcb.fInX)
    {
        settings.flowControl = SerialSettings::FlowControl::Software;
    }
    else if (dcb.fOutxCtsFlow or dcb.fRtsControl == RTS_CONTROL_HANDSHAKE)
    {
        settings.flowControl = SerialSettings::FlowControl::Hardware;
    }
    else
    {
        settings.flowControl = SerialSettings::FlowControl::None;
    }

    return ResultSerialPort(true);
}
} // namespace detail
} // namespace SC
