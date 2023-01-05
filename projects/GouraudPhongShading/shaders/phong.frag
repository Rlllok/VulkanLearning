#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNorm;
layout(location = 3) in vec3 fragPosition;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform Light {
	vec3 position;
	vec3 color;
} light;

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 ambient = 0.1f * light.color;

	vec3 lightDir = normalize(light.position - fragPosition);
	float diff = max(dot(fragNorm, lightDir), 0.0);
	vec3 diffuseLight = diff * light.color;

	outColor = vec4((ambient + diffuseLight) * texture(texSampler, fragTexCoord).rgb, 1.0f);
}