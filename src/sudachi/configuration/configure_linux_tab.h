// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

namespace Core {
class System;
}

namespace Ui {
class ConfigureLinuxTab;
}

namespace ConfigurationShared {
class Builder;
}

class ConfigureLinuxTab : public ConfigurationShared::Tab {
    Q_OBJECT

public:
    explicit ConfigureLinuxTab(const Core::System& system_,
                               std::shared_ptr<std::vector<ConfigurationShared::Tab*>> group,
                               const ConfigurationShared::Builder& builder,
                               QWidget* parent = nullptr);
    ~ConfigureLinuxTab() override;

    void ApplyConfiguration() override;
    void SetConfiguration() override;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void Setup(const ConfigurationShared::Builder& builder);

    std::unique_ptr<Ui::ConfigureLinuxTab> ui;

    const Core::System& system;

    std::vector<std::function<void(bool)>> apply_funcs{};
};
