// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../../../Libraries/Testing/Limits.h"
#include "../../../../Libraries/Time/Time.h"
#include "AsyncUVLoop.inl"

#define SC_ASSERT_PROVIDER AsyncUVAssert
#include "../../../../Libraries/Common/Assert.inl"

#include "AsyncUVHandle.inl"
#include "AsyncUVSignal.inl"
#include "AsyncUVStream.inl"
#include "AsyncUVTcp.inl"
#include "AsyncUVTimer.inl"
#include "AsyncUVUdp.inl"

#include "AsyncUVCheckIdlePrepare.inl"
#include "AsyncUVInet.inl"
#include "AsyncUVMisc.inl"
#include "AsyncUVThread.inl"
