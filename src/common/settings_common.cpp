// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <functional>
#include <string>
#include <vector>
#include "common/settings_common.h"

namespace Settings {

BasicSetting::BasicSetting(Linkage& linkage, const std::string& name, enum Category category_,
                           bool save_, bool runtime_modifiable_, u32 specialization_,
                           BasicSetting* other_setting_)
    : label{name}, category{category_}, id{linkage.count}, save{save_},
      runtime_modifiable{runtime_modifiable_}, specialization{specialization_},
      other_setting{other_setting_} {
    linkage.by_key.insert({name, this});
    linkage.by_category[category].push_back(this);
    linkage.count++;
}

BasicSetting::~BasicSetting() = default;

std::string BasicSetting::ToStringGlobal() const {
    return this->ToString();
}

bool BasicSetting::UsingGlobal() const {
    return true;
}

void BasicSetting::SetGlobal(bool global) {}

bool BasicSetting::Save() const {
    return save;
}

bool BasicSetting::RuntimeModifiable() const {
    return runtime_modifiable;
}

Category BasicSetting::GetCategory() const {
    return category;
}

u32 BasicSetting::Specialization() const {
    return specialization;
}

BasicSetting* BasicSetting::PairedSetting() const {
    return other_setting;
}

const std::string& BasicSetting::GetLabel() const {
    return label;
}

Linkage::Linkage(u32 initial_count) : count{initial_count} {}
Linkage::~Linkage() = default;

} // namespace Settings
