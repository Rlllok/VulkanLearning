#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNorm;

layout(binding = 0) uniform MVP {
    mat4 model;
    mat4 view;
    mat4 projection;
} mvp;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec3 fragPosition;

void main() {
    fragPosition = vec3(mvp.model * vec4(inPosition, 1.0f));
    gl_Position = mvp.projection * mvp.view * mvp.model * vec4(inPosition, 1.0f);
    fragTexCoord = inTexCoord;
    fragNorm = mat3(mvp.model) * inNorm;
}