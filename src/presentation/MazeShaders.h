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
    vec3 light = vec3(0.27, 0.28, 0.30) + exp(-distance * 0.18) * vec3(0.09)
        + cone * exp(-distance * 0.10) * vec3(0.07, 0.068, 0.062);

    for (int i = 0; i < lightCount; ++i) {
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
        if (strength < 0.004 || occluded(samplePoint, panelPoint)) continue;
        light += fixtureColor * strength;
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
    vec3 lit = surface.rgb * light * contact;
    // A gentle highlight shoulder keeps pale wallpaper and ceilings from clipping near a lamp.
    lit = lit * 1.08 / (vec3(1.0) + lit * 0.35);
    finalColor = vec4(mix(lit, fogColor, fog), surface.a);
}
)glsl";

}
