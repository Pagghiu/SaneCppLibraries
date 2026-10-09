// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Memory/String.h"
#include "Libraries/Process/Process.h"

#if SC_PLATFORM_WINDOWS
namespace SerialPortTestHelpers
{
struct WindowsCom0ComPorts
{
    enum class Status
    {
        NotConfigured,
        ConfiguredAndReady,
        ConfiguredButInvalid
    };

    Status status = Status::NotConfigured;

    const char* message = nullptr;

    SC::SmallString<32> portA;
    SC::SmallString<32> portB;

    WindowsCom0ComPorts() : portA(SC::StringEncoding::Ascii), portB(SC::StringEncoding::Ascii) {}
};

static bool isComPortPath(SC::StringSpan path)
{
    SC::SmallString<32> asciiPath(SC::StringEncoding::Ascii);
    if (not asciiPath.assign(path))
    {
        return false;
    }
    SC::StringSpan asciiView = asciiPath.view();
    const char*    parsed    = asciiView.bytesWithoutTerminator();
    SC::size_t     length    = asciiView.sizeInBytes();

    if (length == 0)
    {
        return false;
    }

    if (length >= 4 and parsed[0] == '\\' and parsed[1] == '\\' and parsed[2] == '.' and parsed[3] == '\\')
    {
        parsed += 4;
        length -= 4;
    }

    if (length < 4 or not((parsed[0] == 'C' or parsed[0] == 'c') and (parsed[1] == 'O' or parsed[1] == 'o') and
                          (parsed[2] == 'M' or parsed[2] == 'm')))
    {
        return false;
    }
    parsed += 3;
    length -= 3;
    if (length == 0)
    {
        return false;
    }

    for (SC::size_t idx = 0; idx < length; ++idx)
    {
        const char c = parsed[idx];
        if (c < '0' or c > '9')
        {
            return false;
        }
    }
    return true;
}

static WindowsCom0ComPorts resolveWindowsCom0ComPorts()
{
    WindowsCom0ComPorts    ports;
    SC::ProcessEnvironment environment;
    SC::StringSpan         portAEnv;
    SC::StringSpan         portBEnv;
    const bool             hasPortA = environment.get("SC_TEST_COM0COM_PORT_A", portAEnv) and not portAEnv.isEmpty();
    const bool             hasPortB = environment.get("SC_TEST_COM0COM_PORT_B", portBEnv) and not portBEnv.isEmpty();

    if (not hasPortA and not hasPortB)
    {
        ports.status  = WindowsCom0ComPorts::Status::NotConfigured;
        ports.message = "Skipping real COM loopback: set SC_TEST_COM0COM_PORT_A and SC_TEST_COM0COM_PORT_B";
        return ports;
    }

    if (hasPortA != hasPortB)
    {
        ports.status  = WindowsCom0ComPorts::Status::ConfiguredButInvalid;
        ports.message = "Both SC_TEST_COM0COM_PORT_A and SC_TEST_COM0COM_PORT_B must be set";
        return ports;
    }

    if (not isComPortPath(portAEnv) or not isComPortPath(portBEnv))
    {
        ports.status  = WindowsCom0ComPorts::Status::ConfiguredButInvalid;
        ports.message = "COM ports must be COMx or \\\\.\\COMx";
        return ports;
    }

    if (not ports.portA.assign(portAEnv) or not ports.portB.assign(portBEnv))
    {
        ports.status  = WindowsCom0ComPorts::Status::ConfiguredButInvalid;
        ports.message = "Invalid COM port path length";
        return ports;
    }

    ports.status  = WindowsCom0ComPorts::Status::ConfiguredAndReady;
    ports.message = "Running real COM loopback";
    return ports;
}
} // namespace SerialPortTestHelpers
#endif
