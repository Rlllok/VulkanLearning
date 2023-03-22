#version 450

layout(set = 0, input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
layout(set = 0, input_attachment_index = 1, binding = 1) uniform subpassInput inputNorm;
layout(set = 0, input_attachment_index = 2, binding = 2) uniform subpassInput inputPosition;

layout(set = 1, binding = 0) uniform Light {
	vec3 position;
	vec3 color;
} light;

layout (push_constant) uniform constants
{
	int mode;
} pushConstants;

layout(location = 0) out vec4 outColor;

void main()
{
	if (pushConstants.mode == 1) {
		outColor = vec4(subpassLoad(inputColor).rgb, 1.0f);
		return;
	}
	if (pushConstants.mode == 2) {
		outColor = vec4(subpassLoad(inputNorm).rgb, 1.0f);
		return;
	}
	if (pushConstants.mode == 3) {
		outColor = vec4(subpassLoad(inputPosition).rgb, 1.0f);
		return;
	}

	vec3 ambientLight = 0.1f * light.color;

	vec3 position = subpassLoad(inputPosition).rgb;
	vec3 lightDir = normalize(light.position - position);
	vec3 norm = subpassLoad(inputNorm).rgb;
	float diff = max(dot(norm, lightDir), 0.0f);
	vec3 diffuseLight = diff * light.color;

	vec3 color = subpassLoad(inputColor).rgb;
	outColor = vec4((ambientLight + diffuseLight) * color, 1.0f);
}