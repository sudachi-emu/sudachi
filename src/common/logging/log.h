// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <string_view>

#include <fmt/format.h>

#include "common/logging/formatter.h"
#include "common/logging/types.h"

namespace Common::Log {

// trims up to and including the last of ../, ..\, src/, src\ in a string
constexpr const char* TrimSourcePath(std::string_view source) {
    const auto rfind = [source](const std::string_view match) {
        return source.rfind(match) == source.npos ? 0 : (source.rfind(match) + match.size());
    };
    auto idx = std::max({rfind("src/"), rfind("src\\"), rfind("../"), rfind("..\\")});
    return source.data() + idx;
}

/// Logs a message to the global logger, using fmt
void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename,
                       unsigned int line_num, const char* function, fmt::string_view format,
                       const fmt::format_args& args);

template <typename... Args>
void FmtLogMessage(Class log_class, Level log_level, const char* filename, unsigned int line_num,
                   const char* function, fmt::format_string<Args...> format, const Args&... args) {
    FmtLogMessageImpl(log_class, log_level, filename, line_num, function, format,
                      fmt::make_format_args(args...));
}

} // namespace Common::Log

#ifdef _DEBUG
#define LOG_TRACE(log_class, ...)                                                                  \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Trace,           \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_DEBUG(log_class, ...)                                                                  \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Debug,           \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_INFO(log_class, ...)                                                                   \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Info,            \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_WARNING(log_class, ...)                                                                \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Warning,         \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_ERROR(log_class, ...)                                                                  \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Error,           \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...)                                                               \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Critical,        \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#else
#define LOG_TRACE(log_class, fmt, ...) (void(0))
#define LOG_DEBUG(log_class, fmt, ...) (void(0))
#define LOG_INFO(log_class, ...)                                                                   \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Info,            \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_WARNING(log_class, ...)                                                                \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Warning,         \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_ERROR(log_class, ...)                                                                  \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Error,           \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...)                                                               \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Critical,        \
                               Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,          \
                               __VA_ARGS__)
#endif