#pragma once

namespace MazeLighting {
// GGX / correlated Smith / Schlick material response follows the model described in
// https://google.github.io/filament/main/filament.html. Rectangle reflection and grid visibility
// are bounded real-time approximations, independent of the maze generation random stream.
inline constexpr const char* Fragment = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 worldPosition;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 eyePosition;
uniform vec3 viewDirection;
uniform float emission;
uniform float hdrOutput;
uniform vec4 materialParams; // perceptual roughness, metalness, relief height, clear coat
uniform int wallSurface;
uniform vec3 fogColor;
uniform vec3 fixtureColor;
uniform vec3 bounceColor;
uniform float roomHeight;
uniform vec4 lightPositions[16];
uniform vec4 lightDirections[16];
uniform vec4 lightExtents[16];
uniform int lightCount;
uniform ivec2 wallRows[50];
uniform int mazeSize;
uniform float cellSize;
layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 surfaceData;
const float PI = 3.14159265359;

vec3 linearColor(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

// Narkowicz's CC0 ACES filmic fit; the HDR compositor applies the same display transform.
vec3 displayColor(vec3 c) {
    c = clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), c));
}

bool solid(ivec2 cell) {
    if (cell.x < 0 || cell.y < 0 || cell.x >= mazeSize || cell.y >= mazeSize) return true;
    uint bits = uint(wallRows[cell.y][cell.x / 32]);
    return (bits & (1u << uint(cell.x % 32))) != 0u;
}

bool occluded(vec3 from, vec3 to) {
    vec2 start = from.xz / cellSize + 0.5;
    vec2 finish = to.xz / cellSize + 0.5;
    vec2 ray = finish - start;
    ivec2 cell = ivec2(floor(start)), end = ivec2(floor(finish));
    ivec2 advance = ivec2(sign(ray));
    vec2 stride = 1.0 / max(abs(ray), vec2(0.00001));
    vec2 edge = mix(start - vec2(cell), vec2(cell + 1) - start, step(vec2(0), ray)) * stride;
    if (abs(ray.x) < 0.00001) edge.x = 1e10;
    if (abs(ray.y) < 0.00001) edge.y = 1e10;
    for (int index = 0; index < 8; ++index) {
        if (solid(cell)) return true;
        if (all(equal(cell, end))) return false;
        if (edge.x < edge.y) { cell.x += advance.x; edge.x += stride.x; }
        else { cell.y += advance.y; edge.y += stride.y; }
    }
    return true;
}

float ambientVisibility(vec3 p, vec3 n) {
    if (cellSize <= 0.0 || mazeSize <= 0) return 1.0;
    vec2 grid = p.xz / cellSize + 0.5;
    ivec2 cell = ivec2(floor(grid));
    vec2 local = fract(grid) * cellSize;
    float edge = cellSize;
    if (abs(n.x) < 0.7) {
        if (solid(cell + ivec2(-1, 0))) edge = min(edge, local.x);
        if (solid(cell + ivec2(1, 0))) edge = min(edge, cellSize - local.x);
    }
    if (abs(n.z) < 0.7) {
        if (solid(cell + ivec2(0, -1))) edge = min(edge, local.y);
        if (solid(cell + ivec2(0, 1))) edge = min(edge, cellSize - local.y);
    }
    if (abs(n.y) < 0.7 && roomHeight > 0.0)
        edge = min(edge, min(max(p.y, 0.0), max(roomHeight - p.y, 0.0)));
    return 0.52 + 0.48 * smoothstep(0.015, 0.30, edge);
}

vec3 reliefNormal(vec3 n, float height) {
    vec3 dx = dFdx(worldPosition), dy = dFdy(worldPosition);
    vec3 a = cross(dy, n), b = cross(n, dx);
    float determinant = dot(dx, a);
    vec3 slope = (dFdx(height) * a + dFdy(height) * b)
        * sign(determinant) / max(abs(determinant), 0.000001);
    slope *= min(1.0, 0.18 / max(length(slope), 0.0001));
    return normalize(n - slope);
}

vec3 fresnel(vec3 f0, float v) {
    return f0 + (1.0 - f0) * pow(1.0 - clamp(v, 0.0, 1.0), 5.0);
}

// Store a full-sphere normal in two channels, leaving roughness and reflectance independent.
vec2 encodeNormal(vec3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    vec2 encoded = n.xy;
    if (n.z < 0.0) encoded = (1.0 - abs(encoded.yx)) * sign(encoded + 0.000001);
    return encoded * 0.5 + 0.5;
}

vec3 specularBrdf(vec3 n, vec3 v, vec3 l, float roughness, vec3 f0) {
    float nv = max(dot(n, v), 0.001), nl = max(dot(n, l), 0.001);
    vec3 h = (v + l) / max(length(v + l), 0.0001);
    float nh = max(dot(n, h), 0.0), vh = max(dot(v, h), 0.0);
    float a = roughness * roughness, a2 = a * a;
    float denominator = nh * nh * (a2 - 1.0) + 1.0;
    float distribution = a2 / max(PI * denominator * denominator, 0.000001);
    float smithV = nl * sqrt(nv * nv * (1.0 - a2) + a2);
    float smithL = nv * sqrt(nl * nl * (1.0 - a2) + a2);
    return fresnel(f0, vh) * distribution * 0.5 / max(smithV + smithL, 0.0001);
}

vec3 emitterPoint(vec3 target, vec3 center, vec3 along, vec3 across) {
    vec3 delta = target - center;
    return center + along * clamp(dot(delta, along) / max(dot(along, along), 0.00001), -1.0, 1.0)
        + across * clamp(dot(delta, across) / max(dot(across, across), 0.00001), -1.0, 1.0);
}

float softVisibility(vec3 p, vec3 center, vec3 nearest, vec3 along, vec3 across, float power) {
    ivec2 cell = ivec2(floor(p.xz / cellSize + 0.5));
    ivec2 sourceCell = ivec2(floor(center.xz / cellSize + 0.5));
    if (all(equal(cell, sourceCell)) && !solid(cell)) return 1.0;
    float visible = occluded(p, nearest) ? 0.0 : 1.0;
    if (power < 0.065) return visible;
    visible *= 0.40;
    visible += occluded(p, center + along * 0.7 + across * 0.7) ? 0.0 : 0.15;
    visible += occluded(p, center - along * 0.7 + across * 0.7) ? 0.0 : 0.15;
    visible += occluded(p, center + along * 0.7 - across * 0.7) ? 0.0 : 0.15;
    visible += occluded(p, center - along * 0.7 - across * 0.7) ? 0.0 : 0.15;
    return visible;
}

// Analytical local scattering around each actual fixture; visibility prevents cross-wall shafts.
float lampHaze(int i, vec3 ray, float rayLength) {
    vec3 center = lightPositions[i].xyz, axis = lightDirections[i].xyz;
    float reach = axis.y < -0.5 ? max(roomHeight, 1.0) * 0.90 : 0.85;
    float projected = dot(center - eyePosition, ray);
    if (projected < -reach - 0.5 || projected > rayLength + reach + 0.5) return 0.0;
    vec3 delta = eyePosition - center;
    float alignment = dot(ray, axis);
    float alongRay = clamp((alignment * dot(delta, axis) - dot(delta, ray))
        / max(1.0 - alignment * alignment, 0.015), 0.0, rayLength);
    float alongLamp = clamp(dot(delta + ray * alongRay, axis), 0.0, reach);
    alongRay = clamp(dot(center + axis * alongLamp - eyePosition, ray), 0.0, rayLength);
    vec3 air = eyePosition + ray * alongRay;
    vec3 fromLamp = air - center;
    float depth = dot(fromLamp, axis), width = 0.10 + max(depth, 0.0) * 0.23;
    float radial2 = max(dot(fromLamp, fromLamp) - depth * depth, 0.0);
    float density = exp(-radial2 / (width * width));
    density *= smoothstep(-0.03, 0.10, depth) * (1.0 - smoothstep(reach * 0.55, reach, depth));
    density *= smoothstep(0.0, 0.20, alongRay) * smoothstep(0.0, 0.12, rayLength - alongRay);
    if (density < 0.025 || occluded(air, center)) return 0.0;
    float phase = 0.8775 / pow(max(1.1225 - 0.7 * dot(-ray, axis), 0.1), 1.5);
    return density * lightDirections[i].w * phase * 0.008;
}

void main() {
    surfaceData = vec4(0.5, 0.5, 1.0, 0.0);
    vec4 texel = texture(texture0, fragTexCoord);
    vec4 surface = texel * fragColor * colDiffuse;
    vec3 albedo = linearColor(surface.rgb);
    vec3 offset = worldPosition - eyePosition;
    float distance = max(length(offset), 0.001);
    vec3 view = -offset / distance;
    float fog = 1.0 - exp(-0.045 * max(distance - 4.0, 0.0));
    vec3 fogRadiance = linearColor(fogColor);
    if (emission > 0.5) {
        vec3 radiance = mix(albedo * 4.5, fogRadiance, fog);
        finalColor = vec4(hdrOutput > 0.5 ? radiance : displayColor(radiance), surface.a);
        return;
    }

    vec4 material = materialParams.x > 0.0 ? materialParams : vec4(0.75, 0.0, 0.0, 0.0);
    vec3 geometricNormal = normalize(cross(dFdx(worldPosition), dFdy(worldPosition)));
    if (dot(geometricNormal, view) < 0.0) geometricNormal = -geometricNormal;
    float paneling = (1.0 - smoothstep(0.29, 0.34,
        worldPosition.y / max(roomHeight, 0.01))) * (1.0 - abs(geometricNormal.y));
    // Wall wallpaper transitions to polished lower marble; carpet and ceiling keep their profiles.
    float wallMaterial = float(wallSurface);
    float roughness = mix(material.x, 0.32, paneling * wallMaterial);
    float coat = max(material.w, paneling * wallMaterial * 0.32);
    float relief = dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722)) * material.z;
    relief *= 1.0 - smoothstep(2.0, 8.0, distance);
    vec3 normal = reliefNormal(geometricNormal, relief);
    float variation = dot(dFdx(normal), dFdx(normal)) + dot(dFdy(normal), dFdy(normal));
    roughness = clamp(sqrt(roughness * roughness + min(variation, 0.15)), 0.14, 0.98);
    vec3 samplePoint = worldPosition + geometricNormal * 0.015;
    float ao = ambientVisibility(samplePoint, geometricNormal);
    float viewer = smoothstep(0.48, 0.98, dot(-view, viewDirection));
    vec3 ambient = vec3(0.030, 0.027, 0.024) + vec3(0.012) * (0.5 + 0.5 * normal.y)
        + vec3(0.006) * viewer * exp(-distance * 0.16);
    vec3 f0 = mix(vec3(0.04), albedo, material.y);
    surfaceData = vec4(encodeNormal(normal), roughness,
                       clamp(max(max(f0.r, f0.g), f0.b) + coat * 0.02, 0.0, 1.0));
    vec3 lit = albedo * ambient * ao * (1.0 - material.y);
    lit += fresnel(f0, max(dot(normal, view), 0.0)) * vec3(0.018, 0.014, 0.010) * ao;
    vec3 fixture = linearColor(fixtureColor), bounce = linearColor(bounceColor);
    vec3 haze = vec3(0.0);
    vec3 reflected = reflect(-view, normal);

    for (int i = 0; i < lightCount; ++i) {
        haze += fixture * lampHaze(i, -view, distance);
        vec3 center = lightPositions[i].xyz, facing = lightDirections[i].xyz;
        vec3 along = lightExtents[i].xyz;
        vec3 across = normalize(cross(facing, along)) * lightExtents[i].w;
        vec3 nearest = emitterPoint(samplePoint, center, along, across);
        vec3 delta = nearest - samplePoint;
        float distance2 = dot(delta, delta), range2 = lightPositions[i].w * lightPositions[i].w;
        if (distance2 >= range2) continue;
        vec3 direction = delta * inversesqrt(max(distance2, 0.0001));
        float nl = max(dot(normal, direction), 0.0);
        float window = max(1.0 - distance2 * distance2 / (range2 * range2), 0.0);
        float falloff = window * window / (distance2 + 0.30);
        float spread = facing.y < -0.5
            ? 0.15 + 0.85 * smoothstep(-0.25, 0.65, dot(facing, -direction))
            : 0.65 + 0.35 * max(dot(facing, -direction), 0.0);
        float power = lightDirections[i].w * falloff * spread;
        if (power < 0.004) continue;
        float horizon = dot(normal, center - samplePoint);
        if (horizon + abs(dot(normal, along)) + abs(dot(normal, across)) <= 0.0) continue;
        float visibility = softVisibility(samplePoint, center, nearest, along, across, power);
        if (visibility <= 0.0) continue;
        vec3 radiance = fixture * power * visibility * 2.9;
        vec3 halfVector = (view + direction) / max(length(view + direction), 0.0001);
        vec3 f = fresnel(f0, max(dot(view, halfVector), 0.0));
        float fd90 = 0.5 + 2.0 * roughness * pow(max(dot(direction, halfVector), 0.0), 2.0);
        float diffuse = mix(1.0, fd90, pow(1.0 - nl, 5.0))
            * mix(1.0, fd90, pow(1.0 - max(dot(normal, view), 0.0), 5.0));
        lit += albedo * (1.0 - f) * (1.0 - material.y) * diffuse * nl / PI * radiance;

        // The reflected view ray chooses a point on the finite diffuser, yielding broad source
        // reflections on stone and crisp warm highlights on metal without a reflection texture.
        float denom = dot(reflected, facing);
        float rayT = dot(center - samplePoint, facing) / (abs(denom) > 0.001 ? denom : 0.001);
        vec3 target = rayT > 0.0 ? samplePoint + reflected * rayT : center;
        vec3 specPoint = emitterPoint(target, center, along, across);
        vec3 specDirection = normalize(specPoint - samplePoint);
        float sourceSize = sqrt(dot(along, along) + dot(across, across));
        float specRoughness = clamp(roughness + sourceSize * 0.16 / sqrt(distance2 + 0.1), 0.14, 0.98);
        float energy = pow(roughness / specRoughness, 2.0);
        lit += specularBrdf(normal, view, specDirection, specRoughness, f0)
            * max(dot(normal, specDirection), 0.0) * radiance * energy;
        if (coat > 0.0)
            lit += specularBrdf(normal, view, specDirection, max(0.19, specRoughness * 0.75), vec3(0.04))
                * max(dot(normal, specDirection), 0.0) * radiance * coat * 0.25;
        lit += albedo * bounce * power * visibility * 0.035 * ao * (1.0 - material.y);
        // A broad, weak grazing sheen suggests wool without turning the carpet into a mirror.
        if (material.x > 0.90)
            lit += albedo * radiance * pow(1.0 - max(dot(normal, view), 0.0), 5.0) * nl * 0.025;
    }
    lit *= 0.85 + 0.15 * ao;
    vec3 result = max(mix(lit, fogRadiance, fog) + haze, vec3(0.0));
    finalColor = vec4(hdrOutput > 0.5 ? result : displayColor(result), surface.a);
}
)glsl";
}
