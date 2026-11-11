#pragma once

namespace MazeLighting {
// GGX / correlated Smith / Schlick material response follows the model described in
// https://google.github.io/filament/main/filament.html. Rectangle reflection and grid visibility
// are bounded real-time approximations, independent of the maze generation random stream.
inline constexpr const char* Fragment = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 WorldPosition;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 EyePosition;
uniform vec3 ViewDirection;
uniform float Emission;
uniform float HdrOutput;
uniform vec4 MaterialParams; // perceptual roughness, metalness, relief height, clear coat
uniform int WallSurface;
uniform vec3 FogColor;
uniform vec3 FixtureColor;
uniform vec3 BounceColor;
uniform float RoomHeight;
uniform vec4 LightPositions[16];
uniform vec4 LightDirections[16];
uniform vec4 LightExtents[16];
uniform int LightCount;
uniform ivec2 WallRows[50];
uniform int MazeSize;
uniform float CellSize;
layout(location = 0) out vec4 FinalColor;
layout(location = 1) out vec4 SurfaceData;
const float Pi = 3.14159265359;

// Decode sRGB material color into linear-light RGB using the standard piecewise transfer curve.
// Lighting calculations operate on this result so additions and attenuation represent light
// energy.
vec3 LinearColor(vec3 C) {
    return mix(C / 12.92, pow((C + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), C));
}

// Compress linear HDR radiance with the fitted tone curve and encode it to display sRGB. Use this
// only for direct output; the HDR path defers display conversion to the final composite pass.
vec3 DisplayColor(vec3 C) {
    C = clamp((C * (2.51 * C + 0.03)) / (C * (2.43 * C + 0.59) + 0.14), 0.0, 1.0);
    return mix(C * 12.92, 1.055 * pow(C, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), C));
}

// Read one maze-wall bit from the uploaded row masks, including columns beyond the first 32-bit
// word. Treat cells outside mazeSize as solid to keep light rays inside the maze.
bool Solid(ivec2 Cell) {
    if (Cell.x < 0 || Cell.y < 0 || Cell.x >= MazeSize || Cell.y >= MazeSize) return true;
    uint Bits = uint(WallRows[Cell.y][Cell.x / 32]);
    return (Bits & (1u << uint(Cell.x % 32))) != 0u;
}

// Traverse maze cells along a world-space X/Z segment using grid-boundary distances. Stop at the
// first solid cell; conservatively report occlusion when the bounded traversal cannot reach the
// destination.
bool Occluded(vec3 From, vec3 To) {
    vec2 Start = From.xz / CellSize + 0.5;
    vec2 Finish = To.xz / CellSize + 0.5;
    vec2 Ray = Finish - Start;
    ivec2 Cell = ivec2(floor(Start)), End = ivec2(floor(Finish));
    ivec2 Advance = ivec2(sign(Ray));
    vec2 Stride = 1.0 / max(abs(Ray), vec2(0.00001));
    vec2 Edge = mix(Start - vec2(Cell), vec2(Cell + 1) - Start, step(vec2(0), Ray)) * Stride;
    if (abs(Ray.x) < 0.00001) Edge.x = 1e10;
    if (abs(Ray.y) < 0.00001) Edge.y = 1e10;
    for (int Index = 0; Index < 8; ++Index) {
        if (Solid(Cell)) return true;
        if (all(equal(Cell, End))) return false;
        if (Edge.x < Edge.y) { Cell.x += Advance.x; Edge.x += Stride.x; }
        else { Cell.y += Advance.y; Edge.y += Stride.y; }
    }
    return true;
}

// Estimate local ambient darkening from nearby walls and floor or ceiling edges. Ignore edges
// parallel to the tested surface normal and return full visibility when maze dimensions are
// unavailable.
float AmbientVisibility(vec3 P, vec3 N) {
    if (CellSize <= 0.0 || MazeSize <= 0) return 1.0;
    vec2 Grid = P.xz / CellSize + 0.5;
    ivec2 Cell = ivec2(floor(Grid));
    vec2 Local = fract(Grid) * CellSize;
    float Edge = CellSize;
    if (abs(N.x) < 0.7) {
        if (Solid(Cell + ivec2(-1, 0))) Edge = min(Edge, Local.x);
        if (Solid(Cell + ivec2(1, 0))) Edge = min(Edge, CellSize - Local.x);
    }
    if (abs(N.z) < 0.7) {
        if (Solid(Cell + ivec2(0, -1))) Edge = min(Edge, Local.y);
        if (Solid(Cell + ivec2(0, 1))) Edge = min(Edge, CellSize - Local.y);
    }
    if (abs(N.y) < 0.7 && RoomHeight > 0.0)
        Edge = min(Edge, min(max(P.y, 0.0), max(RoomHeight - P.y, 0.0)));
    return 0.52 + 0.48 * smoothstep(0.015, 0.30, Edge);
}

// Derive a small surface-normal perturbation from world-position and height derivatives. Guard
// near-degenerate determinants and clamp the slope so texture relief cannot create unstable
// extreme normals.
vec3 ReliefNormal(vec3 N, float Height) {
    vec3 Dx = dFdx(WorldPosition), Dy = dFdy(WorldPosition);
    vec3 A = cross(Dy, N), B = cross(N, Dx);
    float determinant = dot(Dx, A);
    vec3 Slope = (dFdx(Height) * A + dFdy(Height) * B)
        * sign(determinant) / max(abs(determinant), 0.000001);
    Slope *= min(1.0, 0.18 / max(length(Slope), 0.0001));
    return normalize(N - Slope);
}

// Approximate angle-dependent reflectance with Schlick's fifth-power curve. f0 is reflectance at
// normal incidence and v is the clamped view/half-vector alignment.
vec3 Fresnel(vec3 F0, float V) {
    return F0 + (1.0 - F0) * pow(1.0 - clamp(V, 0.0, 1.0), 5.0);
}

// Pack a unit normal into two normalized channels with octahedral encoding. The material
// attachment uses the remaining channels for roughness and reflectance read by surface effects.
vec2 EncodeNormal(vec3 N) {
    N /= abs(N.x) + abs(N.y) + abs(N.z);
    vec2 Encoded = N.xy;
    if (N.z < 0.0) Encoded = (1.0 - abs(Encoded.yx)) * sign(Encoded + 0.000001);
    return Encoded * 0.5 + 0.5;
}

// Evaluate the microfacet specular response from a GGX distribution, Smith visibility, and
// Schlick Fresnel. Clamp small denominators to keep grazing-angle highlights finite.
vec3 SpecularBrdf(vec3 N, vec3 V, vec3 L, float Roughness, vec3 F0) {
    float Nv = max(dot(N, V), 0.001), Nl = max(dot(N, L), 0.001);
    vec3 H = (V + L) / max(length(V + L), 0.0001);
    float Nh = max(dot(N, H), 0.0), Vh = max(dot(V, H), 0.0);
    float A = Roughness * Roughness, A2 = A * A;
    float Denominator = Nh * Nh * (A2 - 1.0) + 1.0;
    float Distribution = A2 / max(Pi * Denominator * Denominator, 0.000001);
    float SmithV = Nl * sqrt(Nv * Nv * (1.0 - A2) + A2);
    float SmithL = Nv * sqrt(Nl * Nl * (1.0 - A2) + A2);
    return Fresnel(F0, Vh) * Distribution * 0.5 / max(SmithV + SmithL, 0.0001);
}

// Find the point on a finite rectangular light nearest the supplied target by projecting onto its
// two half-axes. Clamp each projection to the emitter bounds instead of treating the lamp as an
// infinite plane.
vec3 EmitterPoint(vec3 Target, vec3 Center, vec3 Along, vec3 Across) {
    vec3 Delta = Target - Center;
    return Center + Along * clamp(dot(Delta, Along) / max(dot(Along, Along), 0.00001), -1.0, 1.0)
        + Across * clamp(dot(Delta, Across) / max(dot(Across, Across), 0.00001), -1.0, 1.0);
}

// Estimate a rectangular emitter's visibility with bounded wall-occlusion samples. Use a cheaper
// nearest-point test for weak contributions and combine several samples for brighter lights to
// soften shadow transitions.
float SoftVisibility(vec3 P, vec3 Center, vec3 Nearest, vec3 Along, vec3 Across, float Power) {
    ivec2 Cell = ivec2(floor(P.xz / CellSize + 0.5));
    ivec2 SourceCell = ivec2(floor(Center.xz / CellSize + 0.5));
    if (all(equal(Cell, SourceCell)) && !Solid(Cell)) return 1.0;
    float Visible = Occluded(P, Nearest) ? 0.0 : 1.0;
    if (Power < 0.065) return Visible;
    Visible *= 0.40;
    Visible += Occluded(P, Center + Along * 0.7 + Across * 0.7) ? 0.0 : 0.15;
    Visible += Occluded(P, Center - Along * 0.7 + Across * 0.7) ? 0.0 : 0.15;
    Visible += Occluded(P, Center + Along * 0.7 - Across * 0.7) ? 0.0 : 0.15;
    Visible += Occluded(P, Center - Along * 0.7 - Across * 0.7) ? 0.0 : 0.15;
    return Visible;
}

// Estimate light scattering along the camera ray near a fixture's emission axis. Restrict the
// contribution to the visible segment and lamp reach, rejecting haze blocked by maze walls.
float LampHaze(int I, vec3 Ray, float RayLength) {
    vec3 Center = LightPositions[I].xyz, Axis = LightDirections[I].xyz;
    float Reach = Axis.y < -0.5 ? max(RoomHeight, 1.0) * 0.90 : 0.85;
    float Projected = dot(Center - EyePosition, Ray);
    if (Projected < -Reach - 0.5 || Projected > RayLength + Reach + 0.5) return 0.0;
    vec3 Delta = EyePosition - Center;
    float Alignment = dot(Ray, Axis);
    float AlongRay = clamp((Alignment * dot(Delta, Axis) - dot(Delta, Ray))
        / max(1.0 - Alignment * Alignment, 0.015), 0.0, RayLength);
    float AlongLamp = clamp(dot(Delta + Ray * AlongRay, Axis), 0.0, Reach);
    AlongRay = clamp(dot(Center + Axis * AlongLamp - EyePosition, Ray), 0.0, RayLength);
    vec3 Air = EyePosition + Ray * AlongRay;
    vec3 FromLamp = Air - Center;
    float Depth = dot(FromLamp, Axis), Width = 0.10 + max(Depth, 0.0) * 0.23;
    float Radial2 = max(dot(FromLamp, FromLamp) - Depth * Depth, 0.0);
    float Density = exp(-Radial2 / (Width * Width));
    Density *= smoothstep(-0.03, 0.10, Depth) * (1.0 - smoothstep(Reach * 0.55, Reach, Depth));
    Density *= smoothstep(0.0, 0.20, AlongRay) * smoothstep(0.0, 0.12, RayLength - AlongRay);
    if (Density < 0.025 || Occluded(Air, Center)) return 0.0;
    float Phase = 0.8775 / pow(max(1.1225 - 0.7 * dot(-Ray, Axis), 0.1), 1.5);
    return Density * LightDirections[I].w * Phase * 0.008;
}

// Shade a material using linear-light textures, geometric and relief normals, bounded area
// lights, wall shadows, fog, and haze. Write material data for screen-space effects and either
// HDR radiance or display-converted color.
void main() {
    SurfaceData = vec4(0.5, 0.5, 1.0, 0.0);
    vec4 Texel = texture(texture0, fragTexCoord);
    vec4 Surface = Texel * fragColor * colDiffuse;
    vec3 Albedo = LinearColor(Surface.rgb);
    vec3 Offset = WorldPosition - EyePosition;
    float distance = max(length(Offset), 0.001);
    vec3 View = -Offset / distance;
    float Fog = 1.0 - exp(-0.045 * max(distance - 4.0, 0.0));
    vec3 FogRadiance = LinearColor(FogColor);
    if (Emission > 0.5) {
        vec3 Radiance = mix(Albedo * 4.5, FogRadiance, Fog);
        FinalColor = vec4(HdrOutput > 0.5 ? Radiance : DisplayColor(Radiance), Surface.a);
        return;
    }

    vec4 Material = MaterialParams.x > 0.0 ? MaterialParams : vec4(0.75, 0.0, 0.0, 0.0);
    vec3 GeometricNormal = normalize(cross(dFdx(WorldPosition), dFdy(WorldPosition)));
    if (dot(GeometricNormal, View) < 0.0) GeometricNormal = -GeometricNormal;
    float Paneling = (1.0 - smoothstep(0.29, 0.34,
        WorldPosition.y / max(RoomHeight, 0.01))) * (1.0 - abs(GeometricNormal.y));
    // Wall wallpaper transitions to polished lower marble; carpet and ceiling keep their profiles.
    float WallMaterial = float(WallSurface);
    float Roughness = mix(Material.x, 0.32, Paneling * WallMaterial);
    float Coat = max(Material.w, Paneling * WallMaterial * 0.32);
    float Relief = dot(Texel.rgb, vec3(0.2126, 0.7152, 0.0722)) * Material.z;
    Relief *= 1.0 - smoothstep(2.0, 8.0, distance);
    vec3 Normal = ReliefNormal(GeometricNormal, Relief);
    float Variation = dot(dFdx(Normal), dFdx(Normal)) + dot(dFdy(Normal), dFdy(Normal));
    Roughness = clamp(sqrt(Roughness * Roughness + min(Variation, 0.15)), 0.14, 0.98);
    vec3 SamplePoint = WorldPosition + GeometricNormal * 0.015;
    float Ao = AmbientVisibility(SamplePoint, GeometricNormal);
    float Viewer = smoothstep(0.48, 0.98, dot(-View, ViewDirection));
    vec3 Ambient = vec3(0.030, 0.027, 0.024) + vec3(0.012) * (0.5 + 0.5 * Normal.y)
        + vec3(0.006) * Viewer * exp(-distance * 0.16);
    vec3 F0 = mix(vec3(0.04), Albedo, Material.y);
    SurfaceData = vec4(EncodeNormal(Normal), Roughness,
                       clamp(max(max(F0.r, F0.g), F0.b) + Coat * 0.02, 0.0, 1.0));
    vec3 Lit = Albedo * Ambient * Ao * (1.0 - Material.y);
    Lit += Fresnel(F0, max(dot(Normal, View), 0.0)) * vec3(0.018, 0.014, 0.010) * Ao;
    vec3 Fixture = LinearColor(FixtureColor), Bounce = LinearColor(BounceColor);
    vec3 Haze = vec3(0.0);
    vec3 Reflected = reflect(-View, Normal);

    for (int I = 0; I < LightCount; ++I) {
        Haze += Fixture * LampHaze(I, -View, distance);
        vec3 Center = LightPositions[I].xyz, Facing = LightDirections[I].xyz;
        vec3 Along = LightExtents[I].xyz;
        vec3 Across = normalize(cross(Facing, Along)) * LightExtents[I].w;
        vec3 Nearest = EmitterPoint(SamplePoint, Center, Along, Across);
        vec3 Delta = Nearest - SamplePoint;
        float Distance2 = dot(Delta, Delta), Range2 = LightPositions[I].w * LightPositions[I].w;
        if (Distance2 >= Range2) continue;
        vec3 Direction = Delta * inversesqrt(max(Distance2, 0.0001));
        float Nl = max(dot(Normal, Direction), 0.0);
        float Window = max(1.0 - Distance2 * Distance2 / (Range2 * Range2), 0.0);
        float Falloff = Window * Window / (Distance2 + 0.30);
        float Spread = Facing.y < -0.5
            ? 0.15 + 0.85 * smoothstep(-0.25, 0.65, dot(Facing, -Direction))
            : 0.65 + 0.35 * max(dot(Facing, -Direction), 0.0);
        float Power = LightDirections[I].w * Falloff * Spread;
        if (Power < 0.004) continue;
        float Horizon = dot(Normal, Center - SamplePoint);
        if (Horizon + abs(dot(Normal, Along)) + abs(dot(Normal, Across)) <= 0.0) continue;
        float Visibility = SoftVisibility(SamplePoint, Center, Nearest, Along, Across, Power);
        if (Visibility <= 0.0) continue;
        vec3 Radiance = Fixture * Power * Visibility * 2.9;
        vec3 HalfVector = (View + Direction) / max(length(View + Direction), 0.0001);
        vec3 F = Fresnel(F0, max(dot(View, HalfVector), 0.0));
        float Fd90 = 0.5 + 2.0 * Roughness * pow(max(dot(Direction, HalfVector), 0.0), 2.0);
        float Diffuse = mix(1.0, Fd90, pow(1.0 - Nl, 5.0))
            * mix(1.0, Fd90, pow(1.0 - max(dot(Normal, View), 0.0), 5.0));
        Lit += Albedo * (1.0 - F) * (1.0 - Material.y) * Diffuse * Nl / Pi * Radiance;

        // The reflected view ray chooses a point on the finite diffuser, yielding broad source
        // reflections on stone and crisp warm highlights on metal without a reflection texture.
        float Denom = dot(Reflected, Facing);
        float RayT = dot(Center - SamplePoint, Facing) / (abs(Denom) > 0.001 ? Denom : 0.001);
        vec3 Target = RayT > 0.0 ? SamplePoint + Reflected * RayT : Center;
        vec3 SpecPoint = EmitterPoint(Target, Center, Along, Across);
        vec3 SpecDirection = normalize(SpecPoint - SamplePoint);
        float SourceSize = sqrt(dot(Along, Along) + dot(Across, Across));
        float SpecRoughness = clamp(Roughness + SourceSize * 0.16 / sqrt(Distance2 + 0.1), 0.14, 0.98);
        float Energy = pow(Roughness / SpecRoughness, 2.0);
        Lit += SpecularBrdf(Normal, View, SpecDirection, SpecRoughness, F0)
            * max(dot(Normal, SpecDirection), 0.0) * Radiance * Energy;
        if (Coat > 0.0)
            Lit += SpecularBrdf(Normal, View, SpecDirection, max(0.19, SpecRoughness * 0.75), vec3(0.04))
                * max(dot(Normal, SpecDirection), 0.0) * Radiance * Coat * 0.25;
        Lit += Albedo * Bounce * Power * Visibility * 0.035 * Ao * (1.0 - Material.y);
        // A broad, weak grazing sheen suggests wool without turning the carpet into a mirror.
        if (Material.x > 0.90)
            Lit += Albedo * Radiance * pow(1.0 - max(dot(Normal, View), 0.0), 5.0) * Nl * 0.025;
    }
    Lit *= 0.85 + 0.15 * Ao;
    vec3 Result = max(mix(Lit, FogRadiance, Fog) + Haze, vec3(0.0));
    FinalColor = vec4(HdrOutput > 0.5 ? Result : DisplayColor(Result), Surface.a);
}
)glsl";
}
