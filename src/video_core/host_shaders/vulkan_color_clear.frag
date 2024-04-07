// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 460 core

layout (push_constant) uniform PushConstants {
    vec4 clear_color;
};

layout(location = 0) out vec4 color;

void main() {
    color = clear_color;
}
