#version 450

vec2 position[3] = vec2[](
    vec2(3.0f, -1.0f),
    vec2(-1.0f, -1.0f),
    vec2(-1.0f, 3.0f)
);

void main()
{
    gl_Position = vec4(position[gl_VertexIndex], 0.0f, 1.0f);
}