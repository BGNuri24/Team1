#pragma once
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
uniform mat4 matModel;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 worldPosition;
// Pass world positions to the fragment shader so light pools do not depend on polygon size.
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    worldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

inline constexpr const char* Fragment = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 worldPosition;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 eyePosition;
uniform vec3 viewDirection;
uniform float emission;
uniform vec3 fogColor;
uniform vec3 fixtureColor;
uniform vec3 bounceColor;
uniform float roomHeight;
uniform vec4 lightPositions[16]; // xyz: panel center; w: light radius
uniform vec4 lightDirections[16]; // xyz: emission direction; w: strength
uniform vec4 lightExtents[16]; // xyz: long half-axis; w: short half-width
uniform int lightCount;
uniform ivec2 wallRows[50];
uniform int mazeSize;
uniform float cellSize;
out vec4 finalColor;

bool solid(ivec2 cell) {
    if (cell.x < 0 || cell.y < 0 || cell.x >= mazeSize || cell.y >= mazeSize) return true;
    uint bits = uint(wallRows[cell.y][cell.x / 32]);
    return (bits & (1u << uint(cell.x % 32))) != 0u;
}

// Trace only the few cells within a light's radius. Nearby opaque walls block light, even when a
// fixture on the other side of the wall is closer to the camera than the visible fixtures.
bool occluded(vec3 from, vec3 to) {
    vec2 start = from.xz / cellSize + 0.5;
    vec2 finish = to.xz / cellSize + 0.5;
    vec2 ray = finish - start;
    ivec2 cell = ivec2(floor(start));
    ivec2 end = ivec2(floor(finish));
    ivec2 advance = ivec2(sign(ray));
    vec2 stride = 1.0 / max(abs(ray), vec2(0.00001));
    vec2 edge = mix(start - vec2(cell), vec2(cell + 1) - start, step(vec2(0), ray)) * stride;
    if (abs(ray.x) < 0.00001) edge.x = 1e10;
    if (abs(ray.y) < 0.00001) edge.y = 1e10;
    for (int stepIndex = 0; stepIndex < 8; ++stepIndex) {
        if (solid(cell)) return true;
        if (all(equal(cell, end))) return false;
        if (edge.x < edge.y) { cell.x += advance.x; edge.x += stride.x; }
        else { cell.y += advance.y; edge.y += stride.y; }
    }
    return true;
}

// Approximate a thin, softly widening volume under each diffuser. Closest points on the view ray
// and finite lamp axis give a stable haze without a noisy screen-space march or moving particles.
// Tracing from that visible air sample back to the lamp keeps beams out of neighboring rooms.
float lampHaze(int index, vec3 ray, float rayLength) {
    vec3 center = lightPositions[index].xyz;
    vec3 axis = lightDirections[index].xyz;
    float reach = axis.y < -0.5 ? roomHeight * 0.90 : 0.85;
    vec3 delta = eyePosition - center;
    float alignment = dot(ray, axis);
    float alongRay = (alignment * dot(delta, axis) - dot(delta, ray))
        / max(1.0 - alignment * alignment, 0.015);
    alongRay = clamp(alongRay, 0.0, rayLength);
    float alongLamp = clamp(dot(delta + ray * alongRay, axis), 0.0, reach);
    alongRay = clamp(dot(center + axis * alongLamp - eyePosition, ray), 0.0, rayLength);
    vec3 air = eyePosition + ray * alongRay;
    vec3 fromLamp = air - center;
    float depth = dot(fromLamp, axis);
    float width = 0.12 + max(depth, 0.0) * 0.26;
    float radial2 = max(dot(fromLamp, fromLamp) - depth * depth, 0.0);
    float density = exp(-radial2 / (width * width));
    density *= smoothstep(-0.03, 0.10, depth) * (1.0 - smoothstep(reach * 0.55, reach, depth));
    density *= smoothstep(0.0, 0.20, alongRay)
        * smoothstep(0.0, 0.12, rayLength - alongRay);
    if (density < 0.025 || occluded(air, center)) return 0.0;
    return density * lightDirections[index].w * 0.018;
}

void main() {
    vec4 surface = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 offset = worldPosition - eyePosition;
    float distance = length(offset);
    float fog = smoothstep(5.0, 25.0, distance);
    if (emission > 0.5) {
        finalColor = vec4(mix(surface.rgb, fogColor, fog), surface.a);
        return;
    }

    // Derivatives supply correct geometric normals for both custom wall batches and solid props.
    // Face the normal into the room, including when the camera is attached to the ceiling.
    vec3 normal = normalize(cross(dFdx(worldPosition), dFdy(worldPosition)));
    vec3 view = -offset / max(distance, 0.001);
    if (dot(normal, view) < 0.0) normal = -normal;
    vec3 samplePoint = worldPosition + normal * 0.015;
    float cone = smoothstep(0.48, 0.98, dot(-view, viewDirection));
    vec3 light = vec3(0.25, 0.265, 0.29) + exp(-distance * 0.18) * vec3(0.09)
        + cone * exp(-distance * 0.10) * vec3(0.07, 0.068, 0.062);
    vec3 highlights = vec3(0.0);
    vec3 haze = vec3(0.0);
    float paneling = (1.0 - smoothstep(0.29, 0.34, worldPosition.y / max(roomHeight, 0.01)))
        * (1.0 - abs(normal.y));
    // Wallpaper and carpet stay matte; painted lower panels catch a soft satin reflection.
    float gloss = mix(0.045, 0.16, paneling);
    gloss *= mix(1.0, 0.35, abs(normal.y));
    float shininess = mix(28.0, 64.0, paneling);

    for (int i = 0; i < lightCount; ++i) {
        haze += fixtureColor * lampHaze(i, -view, distance);
        vec3 tangent = lightExtents[i].xyz;
        float halfLength2 = dot(tangent, tangent);
        vec3 panelPoint = lightPositions[i].xyz;
        panelPoint += tangent * clamp(dot(samplePoint - panelPoint, tangent) / halfLength2, -1.0, 1.0);
        vec3 across = normalize(cross(lightDirections[i].xyz, tangent)) * lightExtents[i].w;
        panelPoint += across * clamp(dot(samplePoint - panelPoint, across) / dot(across, across), -1.0, 1.0);
        vec3 delta = panelPoint - samplePoint;
        float distance2 = dot(delta, delta);
        float range2 = lightPositions[i].w * lightPositions[i].w;
        if (distance2 >= range2) continue;
        vec3 direction = delta * inversesqrt(max(distance2, 0.0001));
        float diffuse = (max(dot(normal, direction), 0.0) + 0.10) / 1.10;
        float spread = 0.25 + 0.75 * smoothstep(-0.10, 0.70, dot(lightDirections[i].xyz, -direction));
        float falloff = pow(1.0 - distance2 / range2, 2.0) / (1.0 + distance2 * 0.9);
        float strength = lightDirections[i].w * diffuse * spread * falloff;
        if (strength < 0.004) continue;
        // Sample the finite diffuser instead of one point. The stable three-tap penumbra avoids
        // noisy, animated shadow edges and still prevents light passing through maze walls.
        float visibility = occluded(samplePoint, panelPoint) ? 0.0 : 1.0;
        if (strength > 0.025) {
            vec3 wing = tangent * 0.65 + across * 0.65;
            visibility += occluded(samplePoint, lightPositions[i].xyz + wing) ? 0.0 : 1.0;
            visibility += occluded(samplePoint, lightPositions[i].xyz - wing) ? 0.0 : 1.0;
            visibility /= 3.0;
        }
        if (visibility == 0.0) continue;
        strength *= visibility;
        light += fixtureColor * strength;
        // A small palette-tinted bounce fills the shaded side of trim without flattening corners.
        light += bounceColor * strength * 0.12 * (1.0 - max(dot(normal, direction), 0.0));
        vec3 halfway = (direction + view) * inversesqrt(max(dot(direction + view, direction + view), 0.0001));
        float fresnel = 0.18 + 0.82 * pow(1.0 - max(dot(normal, view), 0.0), 5.0);
        float specular = pow(max(dot(normal, halfway), 0.0), shininess);
        highlights += fixtureColor * specular * gloss * (0.5 + fresnel)
            * lightDirections[i].w * falloff * spread * visibility;
    }

    // Slightly deepen floor/ceiling edges against solid walls without marking the floor itself.
    float contact = 1.0;
    if (abs(normal.y) > 0.75) {
        vec2 grid = samplePoint.xz / cellSize + 0.5;
        ivec2 cell = ivec2(floor(grid));
        vec2 local = fract(grid) * cellSize;
        float edge = cellSize;
        if (solid(cell + ivec2(-1, 0))) edge = min(edge, local.x);
        if (solid(cell + ivec2(1, 0))) edge = min(edge, cellSize - local.x);
        if (solid(cell + ivec2(0, -1))) edge = min(edge, local.y);
        if (solid(cell + ivec2(0, 1))) edge = min(edge, cellSize - local.y);
        contact = 0.82 + 0.18 * smoothstep(0.0, 0.22, edge);
    }
    vec3 lit = (surface.rgb * light + highlights) * contact;
    // A gentle highlight shoulder keeps pale wallpaper and ceilings from clipping near a lamp.
    lit = lit * 1.08 / (vec3(1.0) + lit * 0.35);
    finalColor = vec4(mix(lit, fogColor, fog) + haze, surface.a);
}
)glsl";

// Half-resolution separable bloom. The first pass extracts highlights with a soft threshold;
// the second only blurs. The HUD is drawn afterwards and never enters either pass.
inline constexpr const char* Bloom = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 stepSize;
uniform int extractHighlights;
out vec4 finalColor;

vec3 sampleGlow(vec2 uv) {
    vec3 color = texture(texture0, uv).rgb;
    if (extractHighlights == 0) return color;
    float brightness = max(max(color.r, color.g), color.b);
    float knee = clamp(brightness - 0.62, 0.0, 0.24);
    knee = knee * knee / 0.48;
    float contribution = max(brightness - 0.86, knee) / max(brightness, 0.001);
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

}
