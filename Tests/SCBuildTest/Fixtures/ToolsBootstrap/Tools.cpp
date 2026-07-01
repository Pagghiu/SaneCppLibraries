// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../SC.cpp"

#if defined(_WIN32)
extern int bootstrapProbeMain(int argc, const wchar_t* const* argv);

int wmain(int argc, const wchar_t** argv) { return bootstrapProbeMain(argc, argv); }
#else
extern int bootstrapProbeMain(int argc, const char* const* argv);

int main(int argc, const char** argv) { return bootstrapProbeMain(argc, argv); }
#endif
