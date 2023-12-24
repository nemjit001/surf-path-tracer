#version 450
#pragma shader_stage(vertex)

layout(location = 0) out vec2 screenCoords;

void main()
{
	screenCoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(screenCoords * 2.0f + -1.0f, 0.0f, 1.0f);
}
