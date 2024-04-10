// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Core::Frontend {

class Applet {
public:
    virtual ~Applet() = default;
    virtual void Close() const = 0;
};

} // namespace Core::Frontend
