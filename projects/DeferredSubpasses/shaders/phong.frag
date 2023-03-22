#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNorm;
layout(location = 2) in vec3 fragPosition;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNorm;
layout(location = 2) out vec4 outPosition;

void main()
{
	outColor = texture(texSampler, fragTexCoord);
	outNorm = vec4(fragNorm, 1.0f);
	outPosition = vec4(fragPosition, 1.0f);
}