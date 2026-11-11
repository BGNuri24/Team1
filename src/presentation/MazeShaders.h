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
out vec3 WorldPosition;
// Pass vertex UVs and color to the fragment stage and project the already transformed rlgl batch
// position. Do not apply matModel again, because immediate-mode batch vertices already contain
// each primitive's local transform.
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    // rlgl's immediate-mode batch already applies each primitive's local transform on the CPU.
    // Applying matModel again would move an entire full batch by the final primitive's transform.
    WorldPosition = vertexPosition;
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
uniform vec2 StepSize;
uniform int ExtractHighlights;
uniform int LinearHdr;
out vec4 FinalColor;

// Sample the source color and optionally isolate highlights using a soft threshold appropriate
// for HDR or display-space input. Subsequent blur passes reuse the sampled light without
// extracting it again.
vec3 SampleGlow(vec2 Uv) {
    vec3 Color = texture(texture0, Uv).rgb;
    if (ExtractHighlights == 0) return Color;
    float Brightness = max(max(Color.r, Color.g), Color.b);
    float Threshold = LinearHdr != 0 ? 1.0 : 0.86;
    float Soft = LinearHdr != 0 ? 0.50 : 0.24;
    float Knee = clamp(Brightness - Threshold + Soft, 0.0, 2.0 * Soft);
    Knee = Knee * Knee / (4.0 * Soft);
    float Contribution = max(Brightness - Threshold, Knee) / max(Brightness, 0.001);
    return Color * Contribution;
}

// Apply one separable Gaussian blur direction using symmetric texture samples and preselected
// weights. sampleGlow extracts bright radiance only when requested, allowing horizontal and
// vertical passes to share the shader.
void main() {
    vec3 Glow = SampleGlow(fragTexCoord) * 0.227027;
    Glow += SampleGlow(fragTexCoord + StepSize * 1.384615) * 0.316216;
    Glow += SampleGlow(fragTexCoord - StepSize * 1.384615) * 0.316216;
    Glow += SampleGlow(fragTexCoord + StepSize * 3.230769) * 0.070270;
    Glow += SampleGlow(fragTexCoord - StepSize * 3.230769) * 0.070270;
    FinalColor = vec4(Glow, 1.0);
}
)glsl";

// Half-resolution, depth-tested contact occlusion, diffuse bounce and reflected scene rays.
// All inputs belong to this frame: no history buffers, camera trails or moving noise.
inline constexpr const char* SurfaceEffects = R"glsl(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform sampler2D DepthTexture;
uniform sampler2D GeometryTexture;
uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;
uniform mat4 InverseProjection;
out vec4 FinalColor;
const float Pi = 3.14159265359;

// Unpack the material attachment's octahedral normal and transform it into view space.
// Screen-space reflections and ambient sampling use the reconstructed unit direction.
vec3 DecodeNormal(vec2 Encoded) {
    vec2 E = Encoded * 2.0 - 1.0;
    vec3 N = vec3(E, 1.0 - abs(E.x) - abs(E.y));
    if (N.z < 0.0) N.xy = (1.0 - abs(N.yx)) * sign(N.xy + 0.000001);
    return normalize(mat3(ViewMatrix) * normalize(N));
}

// Reconstruct a view-space position from normalized screen coordinates and sampled depth using
// inverseProjection. Divide by homogeneous w to undo the perspective projection.
vec3 PositionAt(vec2 Uv, float Depth) {
    vec4 P = InverseProjection * vec4(Uv * 2.0 - 1.0, Depth * 2.0 - 1.0, 1.0);
    return P.xyz / P.w;
}

// Project a view-space position into normalized screen texture coordinates. Reflection and
// neighborhood probes use these UVs to compare against the scene's depth buffer.
vec2 ProjectPoint(vec3 P) {
    vec4 Clip = ProjectionMatrix * vec4(P, 1.0);
    return Clip.xy / Clip.w * 0.5 + 0.5;
}

// Accept only UVs inside a small inset of the screen boundaries. Screen-space probes outside this
// region are discarded rather than sampling wrapped or unreliable edge data.
bool OnScreen(vec2 Uv) {
    return all(greaterThan(Uv, vec2(0.002))) && all(lessThan(Uv, vec2(0.998)));
}

// March a reflected view-space ray through the scene depth buffer and refine a candidate
// intersection. Return roughness-filtered scene color weighted by Fresnel and confidence, or zero
// when no reliable on-screen hit exists.
vec3 SceneReflection(vec3 P, vec3 N, float Roughness, float Reflectance) {
    if (Roughness > 0.64 || Reflectance <= 0.0) return vec3(0.0);
    vec3 Ray = reflect(normalize(P), N);
    vec3 Start = P + N * 0.025;
    float Previous = 0.04;
    for (int step = 0; step < 24; ++step) {
        float T = 0.08 + float(step) * 0.055 + float(step * step) * 0.008;
        vec3 Q = Start + Ray * T;
        if (Q.z > -0.03) break;
        vec2 Uv = ProjectPoint(Q);
        if (!OnScreen(Uv)) break;
        float Depth = texture(DepthTexture, Uv).r;
        if (Depth >= 0.99999) { Previous = T; continue; }
        float Difference = Q.z - PositionAt(Uv, Depth).z;
        if (Difference < 0.0 && Difference > -(0.12 + T * 0.04)) {
            float Low = Previous, High = T;
            for (int Refine = 0; Refine < 5; ++Refine) {
                float Middle = (Low + High) * 0.5;
                vec3 Probe = Start + Ray * Middle;
                vec2 ProbeUv = ProjectPoint(Probe);
                float SceneZ = PositionAt(ProbeUv, texture(DepthTexture, ProbeUv).r).z;
                if (Probe.z > SceneZ) Low = Middle;
                else High = Middle;
            }
            Uv = ProjectPoint(Start + Ray * High);
            vec3 Hit = PositionAt(Uv, texture(DepthTexture, Uv).r);
            float Error = abs(Hit.z - (Start + Ray * High).z);
            if (High < 0.12 || Error > 0.09 + High * 0.02) return vec3(0.0);
            vec4 HitData = texture(GeometryTexture, Uv);
            float Facing = HitData.a <= 0.0 ? 1.0
                : smoothstep(0.0, 0.25, dot(DecodeNormal(HitData.rg), -Ray));
            vec2 Edge = min(Uv, 1.0 - Uv);
            float Confidence = smoothstep(0.0, 0.10, min(Edge.x, Edge.y)) * Facing;
            Confidence *= 1.0 - smoothstep(3.0, 6.0, High);
            vec2 Blur = (1.0 + Roughness * 7.0) / vec2(textureSize(texture0, 0));
            vec3 Color = texture(texture0, Uv).rgb * 0.40;
            Color += texture(texture0, Uv + vec2(Blur.x, 0)).rgb * 0.15;
            Color += texture(texture0, Uv - vec2(Blur.x, 0)).rgb * 0.15;
            Color += texture(texture0, Uv + vec2(0, Blur.y)).rgb * 0.15;
            Color += texture(texture0, Uv - vec2(0, Blur.y)).rgb * 0.15;
            float Nv = max(dot(N, -normalize(P)), 0.0);
            float Fresnel = Reflectance + (1.0 - Reflectance) * pow(1.0 - Nv, 5.0);
            return min(Color, vec3(8.0)) * Fresnel * (1.0 - Roughness * Roughness)
                * Confidence * 0.65;
        }
        Previous = T;
    }
    return vec3(0.0);
}

// Reconstruct the visible surface, sample a view-space hemisphere for occlusion and color bounce,
// and add a screen-space reflection. Store indirect radiance in RGB and ambient visibility in
// alpha for the compositor.
void main() {
    float Depth = texture(DepthTexture, fragTexCoord).r;
    vec4 Data = texture(GeometryTexture, fragTexCoord);
    if (Depth >= 0.99999 || Data.a <= 0.0) { FinalColor = vec4(0, 0, 0, 1); return; }
    vec3 P = PositionAt(fragTexCoord, Depth), N = DecodeNormal(Data.rg);
    vec3 Helper = abs(N.z) < 0.9 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    vec3 Tangent = normalize(cross(Helper, N)), Bitangent = cross(N, Tangent);
    float Rotation = fract(dot(mod(floor(gl_FragCoord.xy), 4.0), vec2(0.25, 0.5))) * 2.0 * Pi;
    float Blocked = 0.0;
    vec3 Bounce = vec3(0.0);
    for (int SampleIndex = 0; SampleIndex < 12; ++SampleIndex) {
        float Fraction = (float(SampleIndex) + 0.5) / 12.0;
        float Angle = float(SampleIndex) * 2.39996323 + Rotation;
        float Radial = sqrt(1.0 - Fraction * Fraction);
        vec3 Direction = Tangent * cos(Angle) * Radial + Bitangent * sin(Angle) * Radial
            + N * Fraction;
        float Radius = 0.06 + 0.46 * pow(Fraction, 1.4);
        vec3 Probe = P + N * 0.015 + Direction * Radius;
        if (Probe.z > -0.03) continue;
        vec2 Uv = ProjectPoint(Probe);
        if (!OnScreen(Uv)) continue;
        float OtherDepth = texture(DepthTexture, Uv).r;
        if (OtherDepth >= 0.99999) continue;
        vec3 Other = PositionAt(Uv, OtherDepth);
        float Range = 1.0 - smoothstep(0.30, 0.85, abs(P.z - Other.z));
        Blocked += (Other.z > Probe.z + 0.012 ? 1.0 : 0.0) * Range;
        vec3 Delta = Other - P;
        float Separation = length(Delta);
        if (Separation < 0.025 || Separation > 0.85) continue;
        vec3 Incoming = Delta / Separation;
        vec4 OtherData = texture(GeometryTexture, Uv);
        float Emitter = OtherData.a <= 0.0 ? 1.0 : max(dot(DecodeNormal(OtherData.rg), -Incoming), 0.0);
        float Coupling = max(dot(N, Incoming), 0.0) * Emitter;
        Coupling *= 1.0 - smoothstep(0.15, 0.85, Separation);
        Bounce += min(texture(texture0, Uv).rgb, vec3(3.0)) * Coupling;
    }
    vec3 Base = texture(texture0, fragTexCoord).rgb;
    vec3 Pigment = clamp(Base / (dot(Base, vec3(0.2126, 0.7152, 0.0722)) + 0.02), 0.0, 1.0);
    vec3 Reflected = SceneReflection(P, N, Data.b, Data.a);
    FinalColor = vec4(Reflected + Bounce * Pigment * (0.16 / 12.0),
                      clamp(1.0 - Blocked * (0.82 / 12.0), 0.25, 1.0));
}
)glsl";

// Tone-map only the world. UI remains an ordinary sRGB pass drawn after composition.
// The filmic curve is Narkowicz's public-domain fit:
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
inline constexpr const char* Composite = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D BloomTexture;
uniform sampler2D SurfaceTexture;
uniform sampler2D DepthTexture;
uniform mat4 InverseProjection;
uniform int SurfaceEffectsEnabled;
uniform int LinearHdr;
out vec4 FinalColor;

// Reconstruct positive camera-space depth from the depth texture and inverse projection matrix.
// The compositor compares this value across samples to preserve geometry edges during upsampling.
float ViewDepth(vec2 Uv) {
    float Depth = texture(DepthTexture, Uv).r;
    vec4 P = InverseProjection * vec4(Uv * 2.0 - 1.0, Depth * 2.0 - 1.0, 1.0);
    return -P.z / P.w;
}

// Upsample the lower-resolution surface-effect texture using bilinear spatial weights multiplied
// by depth similarity. Reject cross-surface bleeding and return a neutral effect when usable
// sample weight is negligible.
vec4 SurfaceAt(vec2 Uv) {
    vec2 Dimensions = vec2(textureSize(SurfaceTexture, 0));
    vec2 Coordinate = Uv * Dimensions - 0.5;
    vec2 Base = floor(Coordinate), Part = fract(Coordinate);
    float Depth = ViewDepth(Uv), Total = 0.0;
    vec4 Result = vec4(0.0);
    for (int Y = 0; Y < 2; ++Y) for (int X = 0; X < 2; ++X) {
        vec2 Offset = vec2(X, Y);
        vec2 SampleUv = (Base + Offset + 0.5) / Dimensions;
        vec2 Spatial = mix(1.0 - Part, Part, Offset);
        float Weight = Spatial.x * Spatial.y;
        Weight *= exp(-abs(ViewDepth(SampleUv) - Depth) / max(0.02, Depth * 0.015));
        Result += texture(SurfaceTexture, SampleUv) * Weight;
        Total += Weight;
    }
    return Total > 0.0001 ? Result / Total : vec4(0, 0, 0, 1);
}

// Combine the scene with depth-aware surface effects and blurred highlights. Tone-map and
// sRGB-encode only linear HDR input, leaving the final result ready for later screen-space UI
// drawing.
void main() {
    vec3 Scene = texture(texture0, fragTexCoord).rgb;
    if (SurfaceEffectsEnabled != 0) {
        vec4 Detail = SurfaceAt(fragTexCoord);
        Scene = Scene * (0.55 + 0.45 * Detail.a) + Detail.rgb;
    }
    vec3 Bloom = texture(BloomTexture, fragTexCoord).rgb;
    vec3 Color = max(Scene + Bloom * (LinearHdr != 0 ? 0.12 : 0.686), vec3(0.0));
    if (LinearHdr != 0) {
        Color = clamp((Color * (2.51 * Color + 0.03))
            / (Color * (2.43 * Color + 0.59) + 0.14), 0.0, 1.0);
        Color = mix(Color * 12.92, 1.055 * pow(Color, vec3(1.0 / 2.4)) - 0.055,
                    step(vec3(0.0031308), Color));
    }
    FinalColor = vec4(Color, 1.0) * fragColor;
}
)glsl";

}
