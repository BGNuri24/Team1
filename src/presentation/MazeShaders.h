#pragma once
namespace MazeShaders {
// Embed the GLSL programs for interpolated ceiling-light spill, per-fragment view lighting and
// distance fog. Keeping the shader sources here avoids separate shader-file paths or runtime asset
// downloads.
inline constexpr const char* Vertex = R"glsl(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform vec3 ceilingLights[6];
uniform int ceilingLightCount;
uniform vec3 fixtureColor;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 worldPosition;
out vec3 ceilingIllumination;
// Transform the vertex and pass texture coordinates, color, world position and ceiling-light
// contribution to the fragment stage.
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    worldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    // Calculate nearby fixture contributions at each vertex and interpolate their combined illumination
    // across the surface.
    // The fragment shader evaluates view lighting and fog separately, avoiding a fixture loop for every
    // fragment.
    ceilingIllumination = vec3(0.0);
    for (int i = 0; i < ceilingLightCount; ++i) {
        vec3 delta = ceilingLights[i] - worldPosition;
        ceilingIllumination += fixtureColor * 0.48 / (1.0 + dot(delta, delta)*1.4);
    }
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

inline constexpr const char* Fragment = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 worldPosition;
in vec3 ceilingIllumination;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 eyePosition;
uniform vec3 viewDirection;
uniform float emission;
uniform vec3 fogColor;
out vec4 finalColor;
// Shade the sampled surface using ambient, view-directed and ceiling light, then blend distance fog
// while retaining material alpha.
void main() {
    vec4 surface = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 offset = worldPosition - eyePosition;
    float distance = length(offset);
    float cone = smoothstep(0.48, 0.98, dot(offset / max(distance, 0.001), viewDirection));
    float nearby = exp(-distance * 0.18);
    vec3 light = vec3(0.52, 0.52, 0.50) + nearby * vec3(0.24, 0.24, 0.24)
        + cone * exp(-distance * 0.10) * vec3(0.27, 0.26, 0.24);
    light += ceilingIllumination;
    vec3 lit = surface.rgb * mix(light, vec3(1.0), emission);
    float fog = smoothstep(5.0, 25.0, distance);
    finalColor = vec4(mix(lit, fogColor, fog), surface.a);
}
)glsl";

}
