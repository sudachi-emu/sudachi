// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <string>
#include <typeindex>
#include "common/common_types.h"

namespace Settings {

enum class Category : u32 {
    Android,
    Audio,
    Core,
    Cpu,
    CpuDebug,
    CpuUnsafe,
    Overlay,
    Renderer,
    RendererAdvanced,
    RendererDebug,
    System,
    SystemAudio,
    DataStorage,
    Debugging,
    DebuggingGraphics,
    GpuDriver,
    Miscellaneous,
    Network,
    WebService,
    AddOns,
    Controls,
    Ui,
    UiAudio,
    UiGeneral,
    UiLayout,
    UiGameList,
    Screenshots,
    Shortcuts,
    Multiplayer,
    Services,
    Paths,
    Linux,
    LibraryApplet,
    MaxEnum,
};

constexpr u8 SpecializationTypeMask = 0xf;
constexpr u8 SpecializationAttributeMask = 0xf0;
constexpr u8 SpecializationAttributeOffset = 4;

// Scalar and countable could have better names
enum Specialization : u8 {
    Default = 0,
    Time = 1,        // Duration or specific moment in time
    Hex = 2,         // Hexadecimal number
    List = 3,        // Setting has specific members
    RuntimeList = 4, // Members of the list are determined during runtime
    Scalar = 5,      // Values are continuous
    Countable = 6,   // Can be stepped through
    Paired = 7,      // Another setting is associated with this setting
    Radio = 8,       // Setting should be presented in a radio group

    Percentage = (1 << SpecializationAttributeOffset), // Should be represented as a percentage
};

class BasicSetting;

class Linkage {
public:
    explicit Linkage(u32 initial_count = 0);
    ~Linkage();
    std::map<Category, std::vector<BasicSetting*>> by_category{};
    std::map<std::string, Settings::BasicSetting*> by_key{};
    std::vector<std::function<void()>> restore_functions{};
    u32 count;
};

/**
 * BasicSetting is an abstract class that only keeps track of metadata. The string methods are
 * available to get data values out.
 */
class BasicSetting {
protected:
    explicit BasicSetting(Linkage& linkage, const std::string& name, Category category_, bool save_,
                          bool runtime_modifiable_, u32 specialization,
                          BasicSetting* other_setting);

public:
    virtual ~BasicSetting();

    /*
     * Data retrieval
     */

    /**
     * Returns a string representation of the internal data. If the Setting is Switchable, it
     * respects the internal global state: it is based on GetValue().
     *
     * @returns A string representation of the internal data.
     */
    [[nodiscard]] virtual std::string ToString() const = 0;

    /**
     * Returns a string representation of the global version of internal data. If the Setting is
     * not Switchable, it behaves like ToString.
     *
     * @returns A string representation of the global version of internal data.
     */
    [[nodiscard]] virtual std::string ToStringGlobal() const;

    /**
     * @returns A string representation of the Setting's default value.
     */
    [[nodiscard]] virtual std::string DefaultToString() const = 0;

    /**
     * Returns a string representation of the minimum value of the setting. If the Setting is not
     * ranged, the string represents the default initialization of the data type.
     *
     * @returns A string representation of the minimum value of the setting.
     */
    [[nodiscard]] virtual std::string MinVal() const = 0;

    /**
     * Returns a string representation of the maximum value of the setting. If the Setting is not
     * ranged, the string represents the default initialization of the data type.
     *
     * @returns A string representation of the maximum value of the setting.
     */
    [[nodiscard]] virtual std::string MaxVal() const = 0;

    /**
     * Takes a string input, converts it to the internal data type if necessary, and then runs
     * SetValue with it.
     *
     * @param load String of the input data.
     */
    virtual void LoadString(const std::string& load) = 0;

    /**
     * Returns a string representation of the data. If the data is an enum, it returns a string of
     * the enum value. If the internal data type is not an enum, this is equivalent to ToString.
     *
     * e.g. renderer_backend.Canonicalize() == "OpenGL"
     *
     * @returns Canonicalized string representation of the internal data
     */
    [[nodiscard]] virtual std::string Canonicalize() const = 0;

    /*
     * Metadata
     */

    /**
     * @returns A unique identifier for the Setting's internal data type.
     */
    [[nodiscard]] virtual std::type_index TypeId() const = 0;

    /**
     * Returns true if the Setting's internal data type is an enum.
     *
     * @returns True if the Setting's internal data type is an enum
     */
    [[nodiscard]] virtual constexpr bool IsEnum() const = 0;

    /**
     * Returns true if the current setting is Switchable.
     *
     * @returns If the setting is a SwitchableSetting
     */
    [[nodiscard]] virtual constexpr bool Switchable() const {
        return false;
    }

    /**
     * Returns true to suggest that a frontend can read or write the setting to a configuration
     * file.
     *
     * @returns The save preference
     */
    [[nodiscard]] bool Save() const;

    /**
     * @returns true if the current setting can be changed while the guest is running.
     */
    [[nodiscard]] bool RuntimeModifiable() const;

    /**
     * @returns A unique number corresponding to the setting.
     */
    [[nodiscard]] constexpr u32 Id() const {
        return id;
    }

    /**
     * Returns the setting's category AKA INI group.
     *
     * @returns The setting's category
     */
    [[nodiscard]] Category GetCategory() const;

    /**
     * @returns Extra metadata for data representation in frontend implementations.
     */
    [[nodiscard]] u32 Specialization() const;

    /**
     * @returns Another BasicSetting if one is paired, or nullptr otherwise.
     */
    [[nodiscard]] BasicSetting* PairedSetting() const;

    /**
     * Returns the label this setting was created with.
     *
     * @returns A reference to the label
     */
    [[nodiscard]] const std::string& GetLabel() const;

    /**
     * @returns If the Setting checks input values for valid ranges.
     */
    [[nodiscard]] virtual constexpr bool Ranged() const = 0;

    /**
     * @returns The index of the enum if the underlying setting type is an enum, else max of u32.
     */
    [[nodiscard]] virtual constexpr u32 EnumIndex() const = 0;

    /**
     * @returns True if the underlying type is a floating point storage
     */
    [[nodiscard]] virtual constexpr bool IsFloatingPoint() const = 0;

    /**
     * @returns True if the underlying type is an integer storage
     */
    [[nodiscard]] virtual constexpr bool IsIntegral() const = 0;

    /*
     * Switchable settings
     */

    /**
     * Sets a setting's global state. True means use the normal setting, false to use a custom
     * value. Has no effect if the Setting is not Switchable.
     *
     * @param global The desired state
     */
    virtual void SetGlobal(bool global);

    /**
     * Returns true if the setting is using the normal setting value. Always true if the setting is
     * not Switchable.
     *
     * @returns The Setting's global state
     */
    [[nodiscard]] virtual bool UsingGlobal() const;

private:
    const std::string label; ///< The setting's label
    const Category category; ///< The setting's category AKA INI group
    const u32 id;            ///< Unique integer for the setting
    const bool save; ///< Suggests if the setting should be saved and read to a frontend config
    const bool
        runtime_modifiable;   ///< Suggests if the setting can be modified while a guest is running
    const u32 specialization; ///< Extra data to identify representation of a setting
    BasicSetting* const other_setting; ///< A paired setting
};

} // namespace Settings
