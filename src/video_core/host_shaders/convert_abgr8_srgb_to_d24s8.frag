// SPDX-License-Identifier: GPL-2.0-or-later

#version 450
#extension GL_ARB_shader_stencil_export : require

precision highp int;
precision highp float;

layout(binding = 0) uniform sampler2D color_texture;

// Utility function to convert sRGB to linear space
highp float srgbToLinear(highp float srgb) {
    if (srgb <= 0.04045)
        return srgb / 12.92;
    else
        return pow((srgb + 0.055) / 1.055, 2.4);
}

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);

    // Fetch sRGB color and convert to linear space
    vec4 srgbColor = texelFetch(color_texture, coord, 0);
    highp float r_linear = srgbToLinear(srgbColor.r);
    highp float g_linear = srgbToLinear(srgbColor.g);
    highp float b_linear = srgbToLinear(srgbColor.b);

    // Compute luminance for depth
    highp float luminance = 0.2126 * r_linear + 0.7152 * g_linear + 0.0722 * b_linear;
    highp uint depth_val = uint(luminance * (exp2(24.0) - 1.0));

    // Extract the stencil value from the alpha component
    highp uint stencil_val = uint(srgbColor.a * (exp2(8) - 1.0));

    // Pack stencil and depth values into a single uint
    highp uint byte0 = stencil_val << 24;
    highp uint byte1 = (depth_val & 0x00FF0000u) >> 16;
    highp uint byte2 = (depth_val & 0x0000FF00u) >> 8;
    highp uint byte3 = depth_val & 0x000000FFu;
    highp uint depth_stencil_unorm = byte0 | (byte1 << 16) | (byte2 << 8) | byte3;

    // Set depth and stencil values for the fragment
    gl_FragDepth = float(depth_stencil_unorm & 0x00FFFFFFu) / (exp2(24.0) - 1.0);
    gl_FragStencilRefARB = int(depth_stencil_unorm >> 24);
}