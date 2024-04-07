// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <fmt/core.h>
#include "common/common_types.h"
#include "common/settings_common.h"
#include "common/settings_enums.h"

namespace Settings {

/** The Setting class is a simple resource manager. It defines a label and default value
 * alongside the actual value of the setting for simpler and less-error prone use with frontend
 * configurations. Specifying a default value and label is required. A minimum and maximum range
 * can be specified for sanitization.
 */
template <typename Type, bool ranged = false>
class Setting : public BasicSetting {
protected:
    Setting() = default;

public:
    /**
     * Sets a default value, label, and setting value.
     *
     * @param linkage Setting registry
     * @param default_val Initial value of the setting, and default value of the setting
     * @param name Label for the setting
     * @param category_ Category of the setting AKA INI group
     * @param specialization_ Suggestion for how frontend implementations represent this in a config
     * @param save_ Suggests that this should or should not be saved to a frontend config file
     * @param runtime_modifiable_ Suggests whether this is modifiable while a guest is loaded
     * @param other_setting_ A second Setting to associate to this one in metadata
     */
    explicit Setting(Linkage& linkage, const Type& default_val, const std::string& name,
                     Category category_, u32 specialization_ = Specialization::Default,
                     bool save_ = true, bool runtime_modifiable_ = false,
                     BasicSetting* other_setting_ = nullptr)
        requires(!ranged)
        : BasicSetting(linkage, name, category_, save_, runtime_modifiable_, specialization_,
                       other_setting_),
          value{default_val}, default_value{default_val} {}
    virtual ~Setting() = default;

    /**
     * Sets a default value, minimum value, maximum value, and label.
     *
     * @param linkage Setting registry
     * @param default_val Initial value of the setting, and default value of the setting
     * @param min_val Sets the minimum allowed value of the setting
     * @param max_val Sets the maximum allowed value of the setting
     * @param name Label for the setting
     * @param category_ Category of the setting AKA INI group
     * @param specialization_ Suggestion for how frontend implementations represent this in a config
     * @param save_ Suggests that this should or should not be saved to a frontend config file
     * @param runtime_modifiable_ Suggests whether this is modifiable while a guest is loaded
     * @param other_setting_ A second Setting to associate to this one in metadata
     */
    explicit Setting(Linkage& linkage, const Type& default_val, const Type& min_val,
                     const Type& max_val, const std::string& name, Category category_,
                     u32 specialization_ = Specialization::Default, bool save_ = true,
                     bool runtime_modifiable_ = false, BasicSetting* other_setting_ = nullptr)
        requires(ranged)
        : BasicSetting(linkage, name, category_, save_, runtime_modifiable_, specialization_,
                       other_setting_),
          value{default_val}, default_value{default_val}, maximum{max_val}, minimum{min_val} {}

    /**
     *  Returns a reference to the setting's value.
     *
     * @returns A reference to the setting
     */
    [[nodiscard]] virtual const Type& GetValue() const {
        return value;
    }
    [[nodiscard]] virtual const Type& GetValue(bool need_global) const {
        return value;
    }

    /**
     * Sets the setting to the given value.
     *
     * @param val The desired value
     */
    virtual void SetValue(const Type& val) {
        Type temp{ranged ? std::clamp(val, minimum, maximum) : val};
        std::swap(value, temp);
    }

    /**
     * Returns the value that this setting was created with.
     *
     * @returns A reference to the default value
     */
    [[nodiscard]] const Type& GetDefault() const {
        return default_value;
    }

    [[nodiscard]] constexpr bool IsEnum() const override {
        return std::is_enum_v<Type>;
    }

protected:
    [[nodiscard]] std::string ToString(const Type& value_) const {
        if constexpr (std::is_same_v<Type, std::string>) {
            return value_;
        } else if constexpr (std::is_same_v<Type, std::optional<u32>>) {
            return value_.has_value() ? std::to_string(*value_) : "none";
        } else if constexpr (std::is_same_v<Type, bool>) {
            return value_ ? "true" : "false";
        } else if constexpr (std::is_same_v<Type, AudioEngine>) {
            // Compatibility with old AudioEngine setting being a string
            return CanonicalizeEnum(value_);
        } else if constexpr (std::is_floating_point_v<Type>) {
            return fmt::format("{:f}", value_);
        } else if constexpr (std::is_enum_v<Type>) {
            return std::to_string(static_cast<u32>(value_));
        } else {
            return std::to_string(value_);
        }
    }

public:
    /**
     * Converts the value of the setting to a std::string. Respects the global state if the setting
     * has one.
     *
     * @returns The current setting as a std::string
     */
    [[nodiscard]] std::string ToString() const override {
        return ToString(this->GetValue());
    }

    /**
     * Returns the default value of the setting as a std::string.
     *
     * @returns The default value as a string.
     */
    [[nodiscard]] std::string DefaultToString() const override {
        return ToString(default_value);
    }

    /**
     * Assigns a value to the setting.
     *
     * @param val The desired setting value
     *
     * @returns A reference to the setting
     */
    virtual const Type& operator=(const Type& val) {
        Type temp{ranged ? std::clamp(val, minimum, maximum) : val};
        std::swap(value, temp);
        return value;
    }

    /**
     * Returns a reference to the setting.
     *
     * @returns A reference to the setting
     */
    explicit virtual operator const Type&() const {
        return value;
    }

    /**
     * Converts the given value to the Setting's type of value. Uses SetValue to enter the setting,
     * thus respecting its constraints.
     *
     * @param input The desired value
     */
    void LoadString(const std::string& input) override final {
        if (input.empty()) {
            this->SetValue(this->GetDefault());
            return;
        }
        try {
            if constexpr (std::is_same_v<Type, std::string>) {
                this->SetValue(input);
            } else if constexpr (std::is_same_v<Type, std::optional<u32>>) {
                this->SetValue(static_cast<u32>(std::stoul(input)));
            } else if constexpr (std::is_same_v<Type, bool>) {
                this->SetValue(input == "true");
            } else if constexpr (std::is_same_v<Type, float>) {
                this->SetValue(std::stof(input));
            } else if constexpr (std::is_same_v<Type, AudioEngine>) {
                this->SetValue(ToEnum<AudioEngine>(input));
            } else {
                this->SetValue(static_cast<Type>(std::stoll(input)));
            }
        } catch (std::invalid_argument&) {
            this->SetValue(this->GetDefault());
        } catch (std::out_of_range&) {
            this->SetValue(this->GetDefault());
        }
    }

    [[nodiscard]] std::string Canonicalize() const override final {
        if constexpr (std::is_enum_v<Type>) {
            return CanonicalizeEnum(this->GetValue());
        } else {
            return ToString(this->GetValue());
        }
    }

    /**
     * Gives us another way to identify the setting without having to go through a string.
     *
     * @returns the type_index of the setting's type
     */
    [[nodiscard]] std::type_index TypeId() const override final {
        return std::type_index(typeid(Type));
    }

    [[nodiscard]] constexpr u32 EnumIndex() const override final {
        if constexpr (std::is_enum_v<Type>) {
            return EnumMetadata<Type>::Index();
        } else {
            return std::numeric_limits<u32>::max();
        }
    }

    [[nodiscard]] constexpr bool IsFloatingPoint() const final {
        return std::is_floating_point_v<Type>;
    }

    [[nodiscard]] constexpr bool IsIntegral() const final {
        return std::is_integral_v<Type>;
    }

    [[nodiscard]] std::string MinVal() const override final {
        if constexpr (std::is_arithmetic_v<Type> && !ranged) {
            return this->ToString(std::numeric_limits<Type>::min());
        } else {
            return this->ToString(minimum);
        }
    }
    [[nodiscard]] std::string MaxVal() const override final {
        if constexpr (std::is_arithmetic_v<Type> && !ranged) {
            return this->ToString(std::numeric_limits<Type>::max());
        } else {
            return this->ToString(maximum);
        }
    }

    [[nodiscard]] constexpr bool Ranged() const override {
        return ranged;
    }

protected:
    Type value{};               ///< The setting
    const Type default_value{}; ///< The default value
    const Type maximum{};       ///< Maximum allowed value of the setting
    const Type minimum{};       ///< Minimum allowed value of the setting
};

/**
 * The SwitchableSetting class is a slightly more complex version of the Setting class. This adds a
 * custom setting to switch to when a guest application specifically requires it. The effect is that
 * other components of the emulator can access the setting's intended value without any need for the
 * component to ask whether the custom or global setting is needed at the moment.
 *
 * By default, the global setting is used.
 */
template <typename Type, bool ranged = false>
class SwitchableSetting : virtual public Setting<Type, ranged> {
public:
    /**
     * Sets a default value, label, and setting value.
     *
     * @param linkage Setting registry
     * @param default_val Initial value of the setting, and default value of the setting
     * @param name Label for the setting
     * @param category_ Category of the setting AKA INI group
     * @param specialization_ Suggestion for how frontend implementations represent this in a config
     * @param save_ Suggests that this should or should not be saved to a frontend config file
     * @param runtime_modifiable_ Suggests whether this is modifiable while a guest is loaded
     * @param other_setting_ A second Setting to associate to this one in metadata
     */
    template <typename T = BasicSetting>
    explicit SwitchableSetting(Linkage& linkage, const Type& default_val, const std::string& name,
                               Category category_, u32 specialization_ = Specialization::Default,
                               bool save_ = true, bool runtime_modifiable_ = false,
                               typename std::enable_if<!ranged, T*>::type other_setting_ = nullptr)
        : Setting<Type, false>{
              linkage, default_val,         name,          category_, specialization_,
              save_,   runtime_modifiable_, other_setting_} {
        linkage.restore_functions.emplace_back([this]() { this->SetGlobal(true); });
    }
    virtual ~SwitchableSetting() = default;

    /**
     * Sets a default value, minimum value, maximum value, and label.
     *
     * @param linkage Setting registry
     * @param default_val Initial value of the setting, and default value of the setting
     * @param min_val Sets the minimum allowed value of the setting
     * @param max_val Sets the maximum allowed value of the setting
     * @param name Label for the setting
     * @param category_ Category of the setting AKA INI group
     * @param specialization_ Suggestion for how frontend implementations represent this in a config
     * @param save_ Suggests that this should or should not be saved to a frontend config file
     * @param runtime_modifiable_ Suggests whether this is modifiable while a guest is loaded
     * @param other_setting_ A second Setting to associate to this one in metadata
     */
    template <typename T = BasicSetting>
    explicit SwitchableSetting(Linkage& linkage, const Type& default_val, const Type& min_val,
                               const Type& max_val, const std::string& name, Category category_,
                               u32 specialization_ = Specialization::Default, bool save_ = true,
                               bool runtime_modifiable_ = false,
                               typename std::enable_if<ranged, T*>::type other_setting_ = nullptr)
        : Setting<Type, true>{linkage,         default_val, min_val,
                              max_val,         name,        category_,
                              specialization_, save_,       runtime_modifiable_,
                              other_setting_} {
        linkage.restore_functions.emplace_back([this]() { this->SetGlobal(true); });
    }

    /**
     * Tells this setting to represent either the global or custom setting when other member
     * functions are used.
     *
     * @param to_global Whether to use the global or custom setting.
     */
    void SetGlobal(bool to_global) override final {
        use_global = to_global;
    }

    /**
     * Returns whether this setting is using the global setting or not.
     *
     * @returns The global state
     */
    [[nodiscard]] bool UsingGlobal() const override final {
        return use_global;
    }

    /**
     * Returns either the global or custom setting depending on the values of this setting's global
     * state or if the global value was specifically requested.
     *
     * @param need_global Request global value regardless of setting's state; defaults to false
     *
     * @returns The required value of the setting
     */
    [[nodiscard]] const Type& GetValue() const override final {
        if (use_global) {
            return this->value;
        }
        return custom;
    }
    [[nodiscard]] const Type& GetValue(bool need_global) const override final {
        if (use_global || need_global) {
            return this->value;
        }
        return custom;
    }

    /**
     * Sets the current setting value depending on the global state.
     *
     * @param val The new value
     */
    void SetValue(const Type& val) override final {
        Type temp{ranged ? std::clamp(val, this->minimum, this->maximum) : val};
        if (use_global) {
            std::swap(this->value, temp);
        } else {
            std::swap(custom, temp);
        }
    }

    [[nodiscard]] constexpr bool Switchable() const override final {
        return true;
    }

    [[nodiscard]] std::string ToStringGlobal() const override final {
        return this->ToString(this->value);
    }

    /**
     * Assigns the current setting value depending on the global state.
     *
     * @param val The new value
     *
     * @returns A reference to the current setting value
     */
    const Type& operator=(const Type& val) override final {
        Type temp{ranged ? std::clamp(val, this->minimum, this->maximum) : val};
        if (use_global) {
            std::swap(this->value, temp);
            return this->value;
        }
        std::swap(custom, temp);
        return custom;
    }

    /**
     * Returns the current setting value depending on the global state.
     *
     * @returns A reference to the current setting value
     */
    explicit operator const Type&() const override final {
        if (use_global) {
            return this->value;
        }
        return custom;
    }

protected:
    bool use_global{true}; ///< The setting's global state
    Type custom{};         ///< The custom value of the setting
};

} // namespace Settings
