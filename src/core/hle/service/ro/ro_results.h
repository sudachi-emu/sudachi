// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/result.h"

namespace Service::RO {

constexpr Result ResultOutOfAddressSpace{ErrorModule::RO, 2};
constexpr Result ResultAlreadyLoaded{ErrorModule::RO, 3};
constexpr Result ResultInvalidNro{ErrorModule::RO, 4};
constexpr Result ResultInvalidNrr{ErrorModule::RO, 6};
constexpr Result ResultTooManyNro{ErrorModule::RO, 7};
constexpr Result ResultTooManyNrr{ErrorModule::RO, 8};
constexpr Result ResultNotAuthorized{ErrorModule::RO, 9};
constexpr Result ResultInvalidNrrKind{ErrorModule::RO, 10};
constexpr Result ResultInternalError{ErrorModule::RO, 1023};
constexpr Result ResultInvalidAddress{ErrorModule::RO, 1025};
constexpr Result ResultInvalidSize{ErrorModule::RO, 1026};
constexpr Result ResultNotLoaded{ErrorModule::RO, 1028};
constexpr Result ResultNotRegistered{ErrorModule::RO, 1029};
constexpr Result ResultInvalidSession{ErrorModule::RO, 1030};
constexpr Result ResultInvalidProcess{ErrorModule::RO, 1031};

} // namespace Service::RO
