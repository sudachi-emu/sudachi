// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sudachi/configuration/shared_widget.h"

#include <functional>
#include <limits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QValidator>
#include <QVariant>
#include <QtCore/qglobal.h>
#include <QtCore/qobjectdefs.h>
#include <fmt/core.h>
#include <qglobal.h>
#include <qnamespace.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/settings_common.h"
#include "sudachi/configuration/shared_translation.h"

namespace ConfigurationShared {

static int restore_button_count = 0;

static std::string RelevantDefault(const Settings::BasicSetting& setting) {
    return Settings::IsConfiguringGlobal() ? setting.DefaultToString() : setting.ToStringGlobal();
}

static QString DefaultSuffix(QWidget* parent, Settings::BasicSetting& setting) {
    const auto tr = [parent](const char* text, const char* context) {
        return parent->tr(text, context);
    };

    if ((setting.Specialization() & Settings::SpecializationAttributeMask) ==
        Settings::Specialization::Percentage) {
        std::string context{fmt::format("{} percentage (e.g. 50%)", setting.GetLabel())};
        return tr("%", context.c_str());
    }

    return default_suffix;
}

QPushButton* Widget::CreateRestoreGlobalButton(bool using_global, QWidget* parent) {
    restore_button_count++;

    QStyle* style = parent->style();
    QIcon* icon = new QIcon(style->standardIcon(QStyle::SP_LineEditClearButton));
    QPushButton* restore_button = new QPushButton(*icon, QStringLiteral(), parent);
    restore_button->setObjectName(QStringLiteral("RestoreButton%1").arg(restore_button_count));
    restore_button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // Workaround for dark theme causing min-width to be much larger than 0
    restore_button->setStyleSheet(
        QStringLiteral("QAbstractButton#%1 { min-width: 0px }").arg(restore_button->objectName()));

    QSizePolicy sp_retain = restore_button->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    restore_button->setSizePolicy(sp_retain);

    restore_button->setEnabled(!using_global);
    restore_button->setVisible(!using_global);

    return restore_button;
}

QLabel* Widget::CreateLabel(const QString& text) {
    QLabel* qt_label = new QLabel(text, this->parent);
    qt_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return qt_label;
}

QWidget* Widget::CreateCheckBox(Settings::BasicSetting* bool_setting, const QString& label,
                                std::function<std::string()>& serializer,
                                std::function<void()>& restore_func,
                                const std::function<void()>& touch) {
    checkbox = new QCheckBox(label, this);
    checkbox->setCheckState(bool_setting->ToString() == "true" ? Qt::CheckState::Checked
                                                               : Qt::CheckState::Unchecked);
    checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    if (!bool_setting->Save() && !Settings::IsConfiguringGlobal() && runtime_lock) {
        checkbox->setEnabled(false);
    }

    serializer = [this]() {
        return checkbox->checkState() == Qt::CheckState::Checked ? "true" : "false";
    };

    restore_func = [this, bool_setting]() {
        checkbox->setCheckState(RelevantDefault(*bool_setting) == "true" ? Qt::Checked
                                                                         : Qt::Unchecked);
    };

    if (!Settings::IsConfiguringGlobal()) {
        QObject::connect(checkbox, &QCheckBox::clicked, [touch]() { touch(); });
    }

    return checkbox;
}

QWidget* Widget::CreateCombobox(std::function<std::string()>& serializer,
                                std::function<void()>& restore_func,
                                const std::function<void()>& touch) {
    const auto type = setting.EnumIndex();

    combobox = new QComboBox(this);
    combobox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    const ComboboxTranslations* enumeration{nullptr};
    if (combobox_enumerations.contains(type)) {
        enumeration = &combobox_enumerations.at(type);
        for (const auto& [id, name] : *enumeration) {
            combobox->addItem(name);
        }
    } else {
        return combobox;
    }

    const auto find_index = [=](u32 value) -> int {
        for (u32 i = 0; i < enumeration->size(); i++) {
            if (enumeration->at(i).first == value) {
                return i;
            }
        }
        return -1;
    };

    const u32 setting_value = std::strtoul(setting.ToString().c_str(), nullptr, 0);
    combobox->setCurrentIndex(find_index(setting_value));

    serializer = [this, enumeration]() {
        int current = combobox->currentIndex();
        return std::to_string(enumeration->at(current).first);
    };

    restore_func = [this, find_index]() {
        const u32 global_value = std::strtoul(RelevantDefault(setting).c_str(), nullptr, 0);
        combobox->setCurrentIndex(find_index(global_value));
    };

    if (!Settings::IsConfiguringGlobal()) {
        QObject::connect(combobox, QOverload<int>::of(&QComboBox::activated),
                         [touch]() { touch(); });
    }

    return combobox;
}

QWidget* Widget::CreateRadioGroup(std::function<std::string()>& serializer,
                                  std::function<void()>& restore_func,
                                  const std::function<void()>& touch) {
    const auto type = setting.EnumIndex();

    QWidget* group = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    const ComboboxTranslations* enumeration{nullptr};
    if (combobox_enumerations.contains(type)) {
        enumeration = &combobox_enumerations.at(type);
        for (const auto& [id, name] : *enumeration) {
            QRadioButton* radio_button = new QRadioButton(name, group);
            layout->addWidget(radio_button);
            radio_buttons.push_back({id, radio_button});
        }
    } else {
        return group;
    }

    const auto get_selected = [this]() -> int {
        for (const auto& [id, button] : radio_buttons) {
            if (button->isChecked()) {
                return id;
            }
        }
        return -1;
    };

    const auto set_index = [this](u32 value) {
        for (const auto& [id, button] : radio_buttons) {
            button->setChecked(id == value);
        }
    };

    const u32 setting_value = std::strtoul(setting.ToString().c_str(), nullptr, 0);
    set_index(setting_value);

    serializer = [get_selected]() {
        int current = get_selected();
        return std::to_string(current);
    };

    restore_func = [this, set_index]() {
        const u32 global_value = std::strtoul(RelevantDefault(setting).c_str(), nullptr, 0);
        set_index(global_value);
    };

    if (!Settings::IsConfiguringGlobal()) {
        for (const auto& [id, button] : radio_buttons) {
            QObject::connect(button, &QAbstractButton::clicked, [touch]() { touch(); });
        }
    }

    return group;
}

QWidget* Widget::CreateLineEdit(std::function<std::string()>& serializer,
                                std::function<void()>& restore_func,
                                const std::function<void()>& touch, bool managed) {
    const QString text = QString::fromStdString(setting.ToString());
    line_edit = new QLineEdit(this);
    line_edit->setText(text);

    serializer = [this]() { return line_edit->text().toStdString(); };

    if (!managed) {
        return line_edit;
    }

    restore_func = [this]() {
        line_edit->setText(QString::fromStdString(RelevantDefault(setting)));
    };

    if (!Settings::IsConfiguringGlobal()) {
        QObject::connect(line_edit, &QLineEdit::textChanged, [touch]() { touch(); });
    }

    return line_edit;
}

static void CreateIntSlider(Settings::BasicSetting& setting, bool reversed, float multiplier,
                            QLabel* feedback, const QString& use_format, QSlider* slider,
                            std::function<std::string()>& serializer,
                            std::function<void()>& restore_func) {
    const int max_val = std::strtol(setting.MaxVal().c_str(), nullptr, 0);

    const auto update_feedback = [=](int value) {
        int present = (reversed ? max_val - value : value) * multiplier + 0.5f;
        feedback->setText(use_format.arg(QVariant::fromValue(present).value<QString>()));
    };

    QObject::connect(slider, &QAbstractSlider::valueChanged, update_feedback);
    update_feedback(std::strtol(setting.ToString().c_str(), nullptr, 0));

    slider->setMinimum(std::strtol(setting.MinVal().c_str(), nullptr, 0));
    slider->setMaximum(max_val);
    slider->setValue(std::strtol(setting.ToString().c_str(), nullptr, 0));

    serializer = [slider]() { return std::to_string(slider->value()); };
    restore_func = [slider, &setting]() {
        slider->setValue(std::strtol(RelevantDefault(setting).c_str(), nullptr, 0));
    };
}

static void CreateFloatSlider(Settings::BasicSetting& setting, bool reversed, float multiplier,
                              QLabel* feedback, const QString& use_format, QSlider* slider,
                              std::function<std::string()>& serializer,
                              std::function<void()>& restore_func) {
    const float max_val = std::strtof(setting.MaxVal().c_str(), nullptr);
    const float min_val = std::strtof(setting.MinVal().c_str(), nullptr);
    const float use_multiplier =
        multiplier == default_multiplier ? default_float_multiplier : multiplier;

    const auto update_feedback = [=](float value) {
        int present = (reversed ? max_val - value : value) + 0.5f;
        feedback->setText(use_format.arg(QVariant::fromValue(present).value<QString>()));
    };

    QObject::connect(slider, &QAbstractSlider::valueChanged, update_feedback);
    update_feedback(std::strtof(setting.ToString().c_str(), nullptr));

    slider->setMinimum(min_val * use_multiplier);
    slider->setMaximum(max_val * use_multiplier);
    slider->setValue(std::strtof(setting.ToString().c_str(), nullptr) * use_multiplier);

    serializer = [slider, use_multiplier]() {
        return std::to_string(slider->value() / use_multiplier);
    };
    restore_func = [slider, &setting, use_multiplier]() {
        slider->setValue(std::strtof(RelevantDefault(setting).c_str(), nullptr) * use_multiplier);
    };
}

QWidget* Widget::CreateSlider(bool reversed, float multiplier, const QString& given_suffix,
                              std::function<std::string()>& serializer,
                              std::function<void()>& restore_func,
                              const std::function<void()>& touch) {
    if (!setting.Ranged()) {
        LOG_ERROR(Frontend, "\"{}\" is not a ranged setting, but a slider was requested.",
                  setting.GetLabel());
        return nullptr;
    }

    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);

    slider = new QSlider(Qt::Horizontal, this);
    QLabel* feedback = new QLabel(this);

    layout->addWidget(slider);
    layout->addWidget(feedback);

    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->setContentsMargins(0, 0, 0, 0);

    QString suffix = given_suffix == default_suffix ? DefaultSuffix(this, setting) : given_suffix;

    const QString use_format = QStringLiteral("%1").append(suffix);

    if (setting.IsIntegral()) {
        CreateIntSlider(setting, reversed, multiplier, feedback, use_format, slider, serializer,
                        restore_func);
    } else {
        CreateFloatSlider(setting, reversed, multiplier, feedback, use_format, slider, serializer,
                          restore_func);
    }

    slider->setInvertedAppearance(reversed);

    if (!Settings::IsConfiguringGlobal()) {
        QObject::connect(slider, &QAbstractSlider::actionTriggered, [touch]() { touch(); });
    }

    return container;
}

QWidget* Widget::CreateSpinBox(const QString& given_suffix,
                               std::function<std::string()>& serializer,
                               std::function<void()>& restore_func,
                               const std::function<void()>& touch) {
    const auto min_val = std::strtol(setting.MinVal().c_str(), nullptr, 0);
    const auto max_val = std::strtol(setting.MaxVal().c_str(), nullptr, 0);
    const auto default_val = std::strtol(setting.ToString().c_str(), nullptr, 0);

    QString suffix = given_suffix == default_suffix ? DefaultSuffix(this, setting) : given_suffix;

    spinbox = new QSpinBox(this);
    spinbox->setRange(min_val, max_val);
    spinbox->setValue(default_val);
    spinbox->setSuffix(suffix);
    spinbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    serializer = [this]() { return std::to_string(spinbox->value()); };

    restore_func = [this]() {
        auto value{std::strtol(RelevantDefault(setting).c_str(), nullptr, 0)};
        spinbox->setValue(value);
    };

    if (!Settings::IsConfiguringGlobal()) {
        QObject::connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged), [this, touch]() {
            if (spinbox->value() != std::strtol(setting.ToStringGlobal().c_str(), nullptr, 0)) {
                touch();
            }
        });
    }

    return spinbox;
}

QWidget* Widget::CreateDoubleSpinBox(const QString& given_suffix,
                                     std::function<std::string()>& serializer,
                                     std::function<void()>& restore_func,
                                     const std::function<void()>& touch) {
    const auto min_val = std::strtod(setting.MinVal().c_str(), nullptr);
    const auto max_val = std::strtod(setting.MaxVal().c_str(), nullptr);
    const auto default_val = std::strtod(setting.ToString().c_str(), nullptr);

    QString suffix = given_suffix == default_suffix ? DefaultSuffix(this, setting) : given_suffix;

    double_spinbox = new QDoubleSpinBox(this);
    double_spinbox->setRange(min_val, max_val);
    double_spinbox->setValue(default_val);
    double_spinbox->setSuffix(suffix);
    double_spinbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    serializer = [this]() { return fmt::format("{:f}", double_spinbox->value()); };

    restore_func = [this]() {
        auto value{std::strtod(RelevantDefault(setting).c_str(), nullptr)};
        double_spinbox->setValue(value);
    };

    if (!Settings::IsConfiguringGlobal()) {
        QObject::connect(double_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                         [this, touch]() {
                             if (double_spinbox->value() !=
                                 std::strtod(setting.ToStringGlobal().c_str(), nullptr)) {
                                 touch();
                             }
                         });
    }

    return double_spinbox;
}

QWidget* Widget::CreateHexEdit(std::function<std::string()>& serializer,
                               std::function<void()>& restore_func,
                               const std::function<void()>& touch) {
    auto* data_component = CreateLineEdit(serializer, restore_func, touch, false);
    if (data_component == nullptr) {
        return nullptr;
    }

    auto to_hex = [=](const std::string& input) {
        return QString::fromStdString(
            fmt::format("{:08x}", std::strtoul(input.c_str(), nullptr, 0)));
    };

    QRegularExpressionValidator* regex = new QRegularExpressionValidator(
        QRegularExpression{QStringLiteral("^[0-9a-fA-F]{0,8}$")}, line_edit);

    const QString default_val = to_hex(setting.ToString());

    line_edit->setText(default_val);
    line_edit->setMaxLength(8);
    line_edit->setValidator(regex);

    auto hex_to_dec = [this]() -> std::string {
        return std::to_string(std::strtoul(line_edit->text().toStdString().c_str(), nullptr, 16));
    };

    serializer = [hex_to_dec]() { return hex_to_dec(); };

    restore_func = [this, to_hex]() { line_edit->setText(to_hex(RelevantDefault(setting))); };

    if (!Settings::IsConfiguringGlobal()) {

        QObject::connect(line_edit, &QLineEdit::textChanged, [touch]() { touch(); });
    }

    return line_edit;
}

QWidget* Widget::CreateDateTimeEdit(bool disabled, bool restrict,
                                    std::function<std::string()>& serializer,
                                    std::function<void()>& restore_func,
                                    const std::function<void()>& touch) {
    const long long current_time = QDateTime::currentSecsSinceEpoch();
    const s64 the_time =
        disabled ? current_time : std::strtoll(setting.ToString().c_str(), nullptr, 0);
    const auto default_val = QDateTime::fromSecsSinceEpoch(the_time);

    date_time_edit = new QDateTimeEdit(this);
    date_time_edit->setDateTime(default_val);
    date_time_edit->setMinimumDateTime(QDateTime::fromSecsSinceEpoch(0));
    date_time_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    serializer = [this]() { return std::to_string(date_time_edit->dateTime().toSecsSinceEpoch()); };

    auto get_clear_val = [this, restrict, current_time]() {
        return QDateTime::fromSecsSinceEpoch([this, restrict, current_time]() {
            if (restrict && checkbox->checkState() == Qt::Checked) {
                return std::strtoll(RelevantDefault(setting).c_str(), nullptr, 0);
            }
            return current_time;
        }());
    };

    restore_func = [this, get_clear_val]() { date_time_edit->setDateTime(get_clear_val()); };

    if (!Settings::IsConfiguringGlobal()) {
        QObject::connect(date_time_edit, &QDateTimeEdit::editingFinished,
                         [this, get_clear_val, touch]() {
                             if (date_time_edit->dateTime() != get_clear_val()) {
                                 touch();
                             }
                         });
    }

    return date_time_edit;
}

void Widget::SetupComponent(const QString& label, std::function<void()>& load_func, bool managed,
                            RequestType request, float multiplier,
                            Settings::BasicSetting* other_setting, const QString& suffix) {
    created = true;
    const auto type = setting.TypeId();

    QLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    if (other_setting == nullptr) {
        other_setting = setting.PairedSetting();
    }

    const bool require_checkbox =
        other_setting != nullptr && other_setting->TypeId() == typeid(bool);

    if (other_setting != nullptr && other_setting->TypeId() != typeid(bool)) {
        LOG_WARNING(
            Frontend,
            "Extra setting \"{}\" specified but is not bool, refusing to create checkbox for it.",
            other_setting->GetLabel());
    }

    std::function<std::string()> checkbox_serializer = []() -> std::string { return {}; };
    std::function<void()> checkbox_restore_func = []() {};

    std::function<void()> touch = []() {};
    std::function<std::string()> serializer = []() -> std::string { return {}; };
    std::function<void()> restore_func = []() {};

    QWidget* data_component{nullptr};

    request = [&]() {
        if (request != RequestType::Default) {
            return request;
        }
        switch (setting.Specialization() & Settings::SpecializationTypeMask) {
        case Settings::Specialization::Default:
            return RequestType::Default;
        case Settings::Specialization::Time:
            return RequestType::DateTimeEdit;
        case Settings::Specialization::Hex:
            return RequestType::HexEdit;
        case Settings::Specialization::RuntimeList:
            managed = false;
            [[fallthrough]];
        case Settings::Specialization::List:
            return RequestType::ComboBox;
        case Settings::Specialization::Scalar:
            return RequestType::Slider;
        case Settings::Specialization::Countable:
            return RequestType::SpinBox;
        case Settings::Specialization::Radio:
            return RequestType::RadioGroup;
        default:
            break;
        }
        return request;
    }();

    if (!Settings::IsConfiguringGlobal() && managed) {
        restore_button = CreateRestoreGlobalButton(setting.UsingGlobal(), this);

        touch = [this]() {
            LOG_DEBUG(Frontend, "Enabling custom setting for \"{}\"", setting.GetLabel());
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        };
    }

    if (require_checkbox) {
        QWidget* lhs =
            CreateCheckBox(other_setting, label, checkbox_serializer, checkbox_restore_func, touch);
        layout->addWidget(lhs);
    } else if (setting.TypeId() != typeid(bool)) {
        QLabel* qt_label = CreateLabel(label);
        layout->addWidget(qt_label);
    }

    if (setting.TypeId() == typeid(bool)) {
        data_component = CreateCheckBox(&setting, label, serializer, restore_func, touch);
    } else if (setting.IsEnum()) {
        if (request == RequestType::RadioGroup) {
            data_component = CreateRadioGroup(serializer, restore_func, touch);
        } else {
            data_component = CreateCombobox(serializer, restore_func, touch);
        }
    } else if (setting.IsIntegral()) {
        switch (request) {
        case RequestType::Slider:
        case RequestType::ReverseSlider:
            data_component = CreateSlider(request == RequestType::ReverseSlider, multiplier, suffix,
                                          serializer, restore_func, touch);
            break;
        case RequestType::Default:
        case RequestType::LineEdit:
            data_component = CreateLineEdit(serializer, restore_func, touch);
            break;
        case RequestType::DateTimeEdit:
            data_component = CreateDateTimeEdit(other_setting->ToString() != "true", true,
                                                serializer, restore_func, touch);
            break;
        case RequestType::SpinBox:
            data_component = CreateSpinBox(suffix, serializer, restore_func, touch);
            break;
        case RequestType::HexEdit:
            data_component = CreateHexEdit(serializer, restore_func, touch);
            break;
        case RequestType::ComboBox:
            data_component = CreateCombobox(serializer, restore_func, touch);
            break;
        default:
            UNIMPLEMENTED();
        }
    } else if (setting.IsFloatingPoint()) {
        switch (request) {
        case RequestType::Default:
        case RequestType::SpinBox:
            data_component = CreateDoubleSpinBox(suffix, serializer, restore_func, touch);
            break;
        case RequestType::Slider:
        case RequestType::ReverseSlider:
            data_component = CreateSlider(request == RequestType::ReverseSlider, multiplier, suffix,
                                          serializer, restore_func, touch);
            break;
        default:
            UNIMPLEMENTED();
        }
    } else if (type == typeid(std::string)) {
        switch (request) {
        case RequestType::Default:
        case RequestType::LineEdit:
            data_component = CreateLineEdit(serializer, restore_func, touch);
            break;
        case RequestType::ComboBox:
            data_component = CreateCombobox(serializer, restore_func, touch);
            break;
        default:
            UNIMPLEMENTED();
        }
    }

    if (data_component == nullptr) {
        LOG_ERROR(Frontend, "Failed to create widget for \"{}\"", setting.GetLabel());
        created = false;
        return;
    }

    layout->addWidget(data_component);

    if (!managed) {
        return;
    }

    if (Settings::IsConfiguringGlobal()) {
        load_func = [this, serializer, checkbox_serializer, require_checkbox, other_setting]() {
            if (require_checkbox && other_setting->UsingGlobal()) {
                other_setting->LoadString(checkbox_serializer());
            }
            if (setting.UsingGlobal()) {
                setting.LoadString(serializer());
            }
        };
    } else {
        layout->addWidget(restore_button);

        QObject::connect(restore_button, &QAbstractButton::clicked,
                         [this, restore_func, checkbox_restore_func](bool) {
                             LOG_DEBUG(Frontend, "Restore global state for \"{}\"",
                                       setting.GetLabel());

                             restore_button->setEnabled(false);
                             restore_button->setVisible(false);

                             checkbox_restore_func();
                             restore_func();
                         });

        load_func = [this, serializer, require_checkbox, checkbox_serializer, other_setting]() {
            bool using_global = !restore_button->isEnabled();
            setting.SetGlobal(using_global);
            if (!using_global) {
                setting.LoadString(serializer());
            }
            if (require_checkbox) {
                other_setting->SetGlobal(using_global);
                if (!using_global) {
                    other_setting->LoadString(checkbox_serializer());
                }
            }
        };
    }

    if (other_setting != nullptr) {
        const auto reset = [restore_func, data_component](int state) {
            data_component->setEnabled(state == Qt::Checked);
            if (state != Qt::Checked) {
                restore_func();
            }
        };
        connect(checkbox, &QCheckBox::stateChanged, reset);
        reset(checkbox->checkState());
    }
}

bool Widget::Valid() const {
    return created;
}

Widget::~Widget() = default;

Widget::Widget(Settings::BasicSetting* setting_, const TranslationMap& translations_,
               const ComboboxTranslationMap& combobox_translations_, QWidget* parent_,
               bool runtime_lock_, std::vector<std::function<void(bool)>>& apply_funcs_,
               RequestType request, bool managed, float multiplier,
               Settings::BasicSetting* other_setting, const QString& suffix)
    : QWidget(parent_), parent{parent_}, translations{translations_},
      combobox_enumerations{combobox_translations_}, setting{*setting_}, apply_funcs{apply_funcs_},
      runtime_lock{runtime_lock_} {
    if (!Settings::IsConfiguringGlobal() && !setting.Switchable()) {
        LOG_DEBUG(Frontend, "\"{}\" is not switchable, skipping...", setting.GetLabel());
        return;
    }

    const int id = setting.Id();

    const auto [label, tooltip] = [&]() {
        const auto& setting_label = setting.GetLabel();
        if (translations.contains(id)) {
            return std::pair{translations.at(id).first, translations.at(id).second};
        }
        LOG_WARNING(Frontend, "Translation table lacks entry for \"{}\"", setting_label);
        return std::pair{QString::fromStdString(setting_label), QStringLiteral()};
    }();

    if (label == QStringLiteral()) {
        LOG_DEBUG(Frontend, "Translation table has empty entry for \"{}\", skipping...",
                  setting.GetLabel());
        return;
    }

    std::function<void()> load_func = []() {};

    SetupComponent(label, load_func, managed, request, multiplier, other_setting, suffix);

    if (!created) {
        LOG_WARNING(Frontend, "No widget was created for \"{}\"", setting.GetLabel());
        return;
    }

    apply_funcs.push_back([load_func, setting_](bool powered_on) {
        if (setting_->RuntimeModifiable() || !powered_on) {
            load_func();
        }
    });

    bool enable = runtime_lock || setting.RuntimeModifiable();
    if (setting.Switchable() && Settings::IsConfiguringGlobal() && !runtime_lock) {
        enable &= setting.UsingGlobal();
    }
    this->setEnabled(enable);

    this->setToolTip(tooltip);
}

Builder::Builder(QWidget* parent_, bool runtime_lock_)
    : translations{InitializeTranslations(parent_)},
      combobox_translations{ComboboxEnumeration(parent_)}, parent{parent_},
      runtime_lock{runtime_lock_} {}

Builder::~Builder() = default;

Widget* Builder::BuildWidget(Settings::BasicSetting* setting,
                             std::vector<std::function<void(bool)>>& apply_funcs,
                             RequestType request, bool managed, float multiplier,
                             Settings::BasicSetting* other_setting, const QString& suffix) const {
    if (!Settings::IsConfiguringGlobal() && !setting->Switchable()) {
        return nullptr;
    }

    if (setting->Specialization() == Settings::Specialization::Paired) {
        LOG_DEBUG(Frontend, "\"{}\" has specialization Paired: ignoring", setting->GetLabel());
        return nullptr;
    }

    return new Widget(setting, *translations, *combobox_translations, parent, runtime_lock,
                      apply_funcs, request, managed, multiplier, other_setting, suffix);
}

Widget* Builder::BuildWidget(Settings::BasicSetting* setting,
                             std::vector<std::function<void(bool)>>& apply_funcs,
                             Settings::BasicSetting* other_setting, RequestType request,
                             const QString& suffix) const {
    return BuildWidget(setting, apply_funcs, request, true, 1.0f, other_setting, suffix);
}

const ComboboxTranslationMap& Builder::ComboboxTranslations() const {
    return *combobox_translations;
}

} // namespace ConfigurationShared
