#pragma once
#include "MazeLightingShader.h"
namespace MazeShaders {
// Keep the C++ upload limits aligned with the GLSL arrays below. Two 32-bit masks cover each row
// of the supported 10-to-50-cell mazes without uploading a texture or a full shadow map.
inline constexpr int MaxLights = 16;
inline constexpr int MaxMazeRows = 50;

// Embedded shaders keep lighting independent of runtime asset paths.
inline constexpr const char* Vertex = R"glsl(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 worldPosition;
// Pass world positions to the fragment shader so light pools do not depend on polygon size.
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    // rlgl's immediate-mode batch already applies each primitive's local transform on the CPU.
    // Applying matModel again would move an entire full batch by the final primitive's transform.
    worldPosition = vertexPosition;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

inline constexpr const char* Fragment = MazeLighting::Fragment;

// Half-resolution separable bloom. The first pass extracts highlights with a soft threshold;
// the second only blurs. The HUD is drawn afterwards and never enters either pass.
inline constexpr const char* Bloom = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 stepSize;
uniform int extractHighlights;
uniform int linearHdr;
out vec4 finalColor;

vec3 sampleGlow(vec2 uv) {
    vec3 color = texture(texture0, uv).rgb;
    if (extractHighlights == 0) return color;
    float brightness = max(max(color.r, color.g), color.b);
    float threshold = linearHdr != 0 ? 1.0 : 0.86;
    float soft = linearHdr != 0 ? 0.50 : 0.24;
    float knee = clamp(brightness - threshold + soft, 0.0, 2.0 * soft);
    knee = knee * knee / (4.0 * soft);
    float contribution = max(brightness - threshold, knee) / max(brightness, 0.001);
    return color * contribution;
}

void main() {
    vec3 glow = sampleGlow(fragTexCoord) * 0.227027;
    glow += sampleGlow(fragTexCoord + stepSize * 1.384615) * 0.316216;
    glow += sampleGlow(fragTexCoord - stepSize * 1.384615) * 0.316216;
    glow += sampleGlow(fragTexCoord + stepSize * 3.230769) * 0.070270;
    glow += sampleGlow(fragTexCoord - stepSize * 3.230769) * 0.070270;
    finalColor = vec4(glow, 1.0);
}
)glsl";

// Tone-map only the world. UI remains an ordinary sRGB pass drawn after composition.
// The filmic curve is Narkowicz's public-domain fit:
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
inline constexpr const char* Composite = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D bloomTexture;
uniform int linearHdr;
out vec4 finalColor;
void main() {
    vec3 scene = texture(texture0, fragTexCoord).rgb;
    vec3 bloom = texture(bloomTexture, fragTexCoord).rgb;
    vec3 color = max(scene + bloom * (linearHdr != 0 ? 0.12 : 0.686), vec3(0.0));
    if (linearHdr != 0) {
        color = clamp((color * (2.51 * color + 0.03))
            / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
        color = mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055,
                    step(vec3(0.0031308), color));
    }
    finalColor = vec4(color, 1.0) * fragColor;
}
)glsl";

}
