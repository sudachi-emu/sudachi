// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <typeindex>
#include <utility>
#include <vector>
#include <QString>
#include "common/common_types.h"
#include "common/settings.h"

class QWidget;

namespace ConfigurationShared {
using TranslationMap = std::map<u32, std::pair<QString, QString>>;
using ComboboxTranslations = std::vector<std::pair<u32, QString>>;
using ComboboxTranslationMap = std::map<u32, ComboboxTranslations>;

std::unique_ptr<TranslationMap> InitializeTranslations(QWidget* parent);

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QWidget* parent);

static const std::map<Settings::AntiAliasing, QString> anti_aliasing_texts_map = {
    {Settings::AntiAliasing::None, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "None"))},
    {Settings::AntiAliasing::Fxaa, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "FXAA"))},
    {Settings::AntiAliasing::Smaa, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "SMAA"))},
};

static const std::map<Settings::ScalingFilter, QString> scaling_filter_texts_map = {
    {Settings::ScalingFilter::NearestNeighbor,
     QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Nearest"))},
    {Settings::ScalingFilter::Bilinear,
     QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Bilinear"))},
    {Settings::ScalingFilter::Bicubic, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Bicubic"))},
    {Settings::ScalingFilter::Gaussian,
     QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Gaussian"))},
    {Settings::ScalingFilter::ScaleForce,
     QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "ScaleForce"))},
    {Settings::ScalingFilter::Fsr, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "FSR"))},
};

static const std::map<Settings::ConsoleMode, QString> use_docked_mode_texts_map = {
    {Settings::ConsoleMode::Docked, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Docked"))},
    {Settings::ConsoleMode::Handheld, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Handheld"))},
};

static const std::map<Settings::GpuAccuracy, QString> gpu_accuracy_texts_map = {
    {Settings::GpuAccuracy::Normal, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Normal"))},
    {Settings::GpuAccuracy::High, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "High"))},
    {Settings::GpuAccuracy::Extreme, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Extreme"))},
};

static const std::map<Settings::RendererBackend, QString> renderer_backend_texts_map = {
    {Settings::RendererBackend::Vulkan, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Vulkan"))},
    {Settings::RendererBackend::OpenGL, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "OpenGL"))},
    {Settings::RendererBackend::Null, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Null"))},
};

static const std::map<Settings::ShaderBackend, QString> shader_backend_texts_map = {
    {Settings::ShaderBackend::Glsl, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "GLSL"))},
    {Settings::ShaderBackend::Glasm, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "GLASM"))},
    {Settings::ShaderBackend::SpirV, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "SPIRV"))},
};

} // namespace ConfigurationShared
