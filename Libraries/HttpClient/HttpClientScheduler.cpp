// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpClientScheduler.h"

namespace
{
static constexpr SC::size_t InvalidOperationIndex = static_cast<SC::size_t>(-1);
} // namespace

SC::Result SC::HttpClientOperationScheduler::init(const HttpClientOperationSchedulerMemory& memory)
{
    SC_TRY_MSG(not initialized, "HttpClientOperationScheduler: already initialized");
    SC_TRY_MSG(not memory.operations.empty(), "HttpClientOperationScheduler: operations missing");
    SC_TRY_MSG(memory.readyOperations.sizeInElements() >= memory.operations.sizeInElements(),
               "HttpClientOperationScheduler: ready state capacity too small");

    schedulerMemory = memory;
    for (size_t idx = 0; idx < schedulerMemory.operations.sizeInElements(); ++idx)
    {
        SC_TRY_MSG(schedulerMemory.operations[idx] != nullptr, "HttpClientOperationScheduler: null operation");
        SC_TRY_MSG(schedulerMemory.operations[idx]->isInitialized(),
                   "HttpClientOperationScheduler: operation not initialized");
        schedulerMemory.readyOperations[idx] = 1;
        schedulerMemory.operations[idx]->setNotifier(this);
    }

    initialized = true;
    readyCV.broadcast();
    return Result(true);
}

SC::Result SC::HttpClientOperationScheduler::close()
{
    if (not initialized)
    {
        return Result(true);
    }

    for (size_t idx = 0; idx < schedulerMemory.operations.sizeInElements(); ++idx)
    {
        if (schedulerMemory.operations[idx] != nullptr)
        {
            schedulerMemory.operations[idx]->setNotifier(nullptr);
        }
        schedulerMemory.readyOperations[idx] = 0;
    }
    initialized = false;
    readyCV.broadcast();
    return Result(true);
}

SC::size_t SC::HttpClientOperationScheduler::findOperationIndex(HttpClientOperation& operation) const
{
    for (size_t idx = 0; idx < schedulerMemory.operations.sizeInElements(); ++idx)
    {
        if (schedulerMemory.operations[idx] == &operation)
        {
            return idx;
        }
    }
    return InvalidOperationIndex;
}

bool SC::HttpClientOperationScheduler::hasReadyOperationLocked() const
{
    for (size_t idx = 0; idx < schedulerMemory.operations.sizeInElements(); ++idx)
    {
        if (schedulerMemory.readyOperations[idx] != 0)
        {
            return true;
        }
    }
    return false;
}

SC::Result SC::HttpClientOperationScheduler::markReady(HttpClientOperation& operation)
{
    SC_TRY_MSG(initialized, "HttpClientOperationScheduler: not initialized");

    const size_t index = findOperationIndex(operation);
    SC_TRY_MSG(index != InvalidOperationIndex, "HttpClientOperationScheduler: operation not registered");

    readyMutex.lock();
    schedulerMemory.readyOperations[index] = 1;
    readyCV.broadcast();
    readyMutex.unlock();
    return Result(true);
}

void SC::HttpClientOperationScheduler::notifyHttpClientOperation(HttpClientOperation& operation)
{
    if (not initialized)
    {
        return;
    }

    const size_t index = findOperationIndex(operation);
    if (index == InvalidOperationIndex)
    {
        return;
    }

    readyMutex.lock();
    schedulerMemory.readyOperations[index] = 1;
    readyCV.broadcast();
    readyMutex.unlock();
}

SC::Result SC::HttpClientOperationScheduler::pollReady(size_t& numPolled, uint32_t waitMilliseconds)
{
    SC_TRY_MSG(initialized, "HttpClientOperationScheduler: not initialized");

    numPolled = 0;

    readyMutex.lock();
    if (not hasReadyOperationLocked() and waitMilliseconds > 0 and hasRequestsInFlight())
    {
        (void)readyCV.wait(readyMutex, waitMilliseconds);
    }
    readyMutex.unlock();

    for (size_t idx = 0; idx < schedulerMemory.operations.sizeInElements(); ++idx)
    {
        bool shouldPoll = false;
        readyMutex.lock();
        if (schedulerMemory.readyOperations[idx] != 0)
        {
            schedulerMemory.readyOperations[idx] = 0;
            shouldPoll                           = true;
        }
        readyMutex.unlock();

        if (shouldPoll)
        {
            SC_TRY(schedulerMemory.operations[idx]->poll(0));
            numPolled += 1;
        }
    }
    return Result(true);
}

SC::Result SC::HttpClientOperationScheduler::pollAll(size_t& numPolled)
{
    SC_TRY_MSG(initialized, "HttpClientOperationScheduler: not initialized");

    numPolled = 0;
    for (size_t idx = 0; idx < schedulerMemory.operations.sizeInElements(); ++idx)
    {
        SC_TRY(schedulerMemory.operations[idx]->poll(0));
        numPolled += 1;
    }
    return Result(true);
}

SC::size_t SC::HttpClientOperationScheduler::getNumReadyOperations() const
{
    if (not initialized)
    {
        return 0;
    }

    size_t count = 0;
    readyMutex.lock();
    for (size_t idx = 0; idx < schedulerMemory.operations.sizeInElements(); ++idx)
    {
        if (schedulerMemory.readyOperations[idx] != 0)
        {
            count += 1;
        }
    }
    readyMutex.unlock();
    return count;
}

SC::size_t SC::HttpClientOperationScheduler::getNumOperations() const
{
    if (not initialized)
    {
        return 0;
    }
    return schedulerMemory.operations.sizeInElements();
}

bool SC::HttpClientOperationScheduler::isOperationRegistered(HttpClientOperation& operation) const
{
    return initialized and findOperationIndex(operation) != InvalidOperationIndex;
}

SC::size_t SC::HttpClientOperationScheduler::getNumRequestsInFlight() const
{
    if (not initialized)
    {
        return 0;
    }

    size_t count = 0;

    for (size_t idx = 0; idx < schedulerMemory.operations.sizeInElements(); ++idx)
    {
        if (schedulerMemory.operations[idx]->isRequestInFlight())
        {
            count += 1;
        }
    }
    return count;
}

bool SC::HttpClientOperationScheduler::hasRequestsInFlight() const { return getNumRequestsInFlight() > 0; }
