#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec2 screenCoords;

layout(location = 0) out vec4 fragColor;

void main()
{
	fragColor = vec4(screenCoords, 0, 1);
}
