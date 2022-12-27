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

layout(binding = 2) uniform Light {
	vec3 position;
	vec3 color;
} light;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = mvp.projection * mvp.view * mvp.model * vec4(inPosition, 1.0f);

    vec3 ambientLight = 0.1f * light.color;

    vec3 lightDir = normalize(light.position - inPosition);
    vec3 norm = mat3(mvp.model) * inNorm;
    float diff = max(dot(lightDir, norm), 0.0f);
    vec3 diffuseLight = diff * light.color;

    fragColor = vec4(ambientLight + diffuseLight, 1.0f);
    
    fragTexCoord = inTexCoord;
}