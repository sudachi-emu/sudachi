// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef _WIN32

#include <signal.h>

namespace Common {

// Android's ART overrides sigaction with its own wrapper. This is problematic for SIGSEGV
// in particular, because ART's handler accesses tpidr_el0, which conflicts with NCE.
// This extracts the libc symbol and calls it directly.
int SigAction(int signum, const struct sigaction* act, struct sigaction* oldact);

} // namespace Common

#endif
