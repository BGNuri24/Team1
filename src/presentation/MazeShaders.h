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

// Half-resolution, depth-tested contact occlusion, diffuse bounce and reflected scene rays.
// All inputs belong to this frame: no history buffers, camera trails or moving noise.
inline constexpr const char* SurfaceEffects = R"glsl(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform sampler2D depthTexture;
uniform sampler2D geometryTexture;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 inverseProjection;
out vec4 finalColor;
const float PI = 3.14159265359;

vec3 decodeNormal(vec2 encoded) {
    vec2 e = encoded * 2.0 - 1.0;
    vec3 n = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * sign(n.xy + 0.000001);
    return normalize(mat3(viewMatrix) * normalize(n));
}

vec3 positionAt(vec2 uv, float depth) {
    vec4 p = inverseProjection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

vec2 projectPoint(vec3 p) {
    vec4 clip = projectionMatrix * vec4(p, 1.0);
    return clip.xy / clip.w * 0.5 + 0.5;
}

bool onScreen(vec2 uv) {
    return all(greaterThan(uv, vec2(0.002))) && all(lessThan(uv, vec2(0.998)));
}

vec3 sceneReflection(vec3 p, vec3 n, float roughness, float reflectance) {
    if (roughness > 0.64 || reflectance <= 0.0) return vec3(0.0);
    vec3 ray = reflect(normalize(p), n);
    vec3 start = p + n * 0.025;
    float previous = 0.04;
    for (int step = 0; step < 24; ++step) {
        float t = 0.08 + float(step) * 0.055 + float(step * step) * 0.008;
        vec3 q = start + ray * t;
        if (q.z > -0.03) break;
        vec2 uv = projectPoint(q);
        if (!onScreen(uv)) break;
        float depth = texture(depthTexture, uv).r;
        if (depth >= 0.99999) { previous = t; continue; }
        float difference = q.z - positionAt(uv, depth).z;
        if (difference < 0.0 && difference > -(0.12 + t * 0.04)) {
            float low = previous, high = t;
            for (int refine = 0; refine < 5; ++refine) {
                float middle = (low + high) * 0.5;
                vec3 probe = start + ray * middle;
                vec2 probeUv = projectPoint(probe);
                float sceneZ = positionAt(probeUv, texture(depthTexture, probeUv).r).z;
                if (probe.z > sceneZ) low = middle;
                else high = middle;
            }
            uv = projectPoint(start + ray * high);
            vec3 hit = positionAt(uv, texture(depthTexture, uv).r);
            float error = abs(hit.z - (start + ray * high).z);
            if (high < 0.12 || error > 0.09 + high * 0.02) return vec3(0.0);
            vec4 hitData = texture(geometryTexture, uv);
            float facing = hitData.a <= 0.0 ? 1.0
                : smoothstep(0.0, 0.25, dot(decodeNormal(hitData.rg), -ray));
            vec2 edge = min(uv, 1.0 - uv);
            float confidence = smoothstep(0.0, 0.10, min(edge.x, edge.y)) * facing;
            confidence *= 1.0 - smoothstep(3.0, 6.0, high);
            vec2 blur = (1.0 + roughness * 7.0) / vec2(textureSize(texture0, 0));
            vec3 color = texture(texture0, uv).rgb * 0.40;
            color += texture(texture0, uv + vec2(blur.x, 0)).rgb * 0.15;
            color += texture(texture0, uv - vec2(blur.x, 0)).rgb * 0.15;
            color += texture(texture0, uv + vec2(0, blur.y)).rgb * 0.15;
            color += texture(texture0, uv - vec2(0, blur.y)).rgb * 0.15;
            float nv = max(dot(n, -normalize(p)), 0.0);
            float fresnel = reflectance + (1.0 - reflectance) * pow(1.0 - nv, 5.0);
            return min(color, vec3(8.0)) * fresnel * (1.0 - roughness * roughness)
                * confidence * 0.65;
        }
        previous = t;
    }
    return vec3(0.0);
}

void main() {
    float depth = texture(depthTexture, fragTexCoord).r;
    vec4 data = texture(geometryTexture, fragTexCoord);
    if (depth >= 0.99999 || data.a <= 0.0) { finalColor = vec4(0, 0, 0, 1); return; }
    vec3 p = positionAt(fragTexCoord, depth), n = decodeNormal(data.rg);
    vec3 helper = abs(n.z) < 0.9 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    vec3 tangent = normalize(cross(helper, n)), bitangent = cross(n, tangent);
    float rotation = fract(dot(mod(floor(gl_FragCoord.xy), 4.0), vec2(0.25, 0.5))) * 2.0 * PI;
    float blocked = 0.0;
    vec3 bounce = vec3(0.0);
    for (int sampleIndex = 0; sampleIndex < 12; ++sampleIndex) {
        float fraction = (float(sampleIndex) + 0.5) / 12.0;
        float angle = float(sampleIndex) * 2.39996323 + rotation;
        float radial = sqrt(1.0 - fraction * fraction);
        vec3 direction = tangent * cos(angle) * radial + bitangent * sin(angle) * radial
            + n * fraction;
        float radius = 0.06 + 0.46 * pow(fraction, 1.4);
        vec3 probe = p + n * 0.015 + direction * radius;
        if (probe.z > -0.03) continue;
        vec2 uv = projectPoint(probe);
        if (!onScreen(uv)) continue;
        float otherDepth = texture(depthTexture, uv).r;
        if (otherDepth >= 0.99999) continue;
        vec3 other = positionAt(uv, otherDepth);
        float range = 1.0 - smoothstep(0.30, 0.85, abs(p.z - other.z));
        blocked += (other.z > probe.z + 0.012 ? 1.0 : 0.0) * range;
        vec3 delta = other - p;
        float separation = length(delta);
        if (separation < 0.025 || separation > 0.85) continue;
        vec3 incoming = delta / separation;
        vec4 otherData = texture(geometryTexture, uv);
        float emitter = otherData.a <= 0.0 ? 1.0 : max(dot(decodeNormal(otherData.rg), -incoming), 0.0);
        float coupling = max(dot(n, incoming), 0.0) * emitter;
        coupling *= 1.0 - smoothstep(0.15, 0.85, separation);
        bounce += min(texture(texture0, uv).rgb, vec3(3.0)) * coupling;
    }
    vec3 base = texture(texture0, fragTexCoord).rgb;
    vec3 pigment = clamp(base / (dot(base, vec3(0.2126, 0.7152, 0.0722)) + 0.02), 0.0, 1.0);
    vec3 reflected = sceneReflection(p, n, data.b, data.a);
    finalColor = vec4(reflected + bounce * pigment * (0.16 / 12.0),
                      clamp(1.0 - blocked * (0.82 / 12.0), 0.25, 1.0));
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
uniform sampler2D surfaceTexture;
uniform sampler2D depthTexture;
uniform mat4 inverseProjection;
uniform int surfaceEffectsEnabled;
uniform int linearHdr;
out vec4 finalColor;

float viewDepth(vec2 uv) {
    float depth = texture(depthTexture, uv).r;
    vec4 p = inverseProjection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return -p.z / p.w;
}

// Depth-aware reconstruction keeps half-resolution contact shadows inside their actual edges.
vec4 surfaceAt(vec2 uv) {
    vec2 dimensions = vec2(textureSize(surfaceTexture, 0));
    vec2 coordinate = uv * dimensions - 0.5;
    vec2 base = floor(coordinate), part = fract(coordinate);
    float depth = viewDepth(uv), total = 0.0;
    vec4 result = vec4(0.0);
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
        vec2 offset = vec2(x, y);
        vec2 sampleUv = (base + offset + 0.5) / dimensions;
        vec2 spatial = mix(1.0 - part, part, offset);
        float weight = spatial.x * spatial.y;
        weight *= exp(-abs(viewDepth(sampleUv) - depth) / max(0.02, depth * 0.015));
        result += texture(surfaceTexture, sampleUv) * weight;
        total += weight;
    }
    return total > 0.0001 ? result / total : vec4(0, 0, 0, 1);
}

void main() {
    vec3 scene = texture(texture0, fragTexCoord).rgb;
    if (surfaceEffectsEnabled != 0) {
        vec4 detail = surfaceAt(fragTexCoord);
        scene = scene * (0.55 + 0.45 * detail.a) + detail.rgb;
    }
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
