#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec2 screenCoords;

layout(location = 0) out vec4 fragColor;

layout(binding = 0, set = 0) uniform sampler2D renderResult;

void main()
{
	fragColor = texture(renderResult, screenCoords);
}
