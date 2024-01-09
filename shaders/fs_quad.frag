#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec2 screenCoords;

layout(location = 0) out vec4 fragColor;

layout(binding = 0, set = 0) uniform sampler2D renderResult;

void main()
{
	vec4 linearColor = texture(renderResult, screenCoords);	// output is in linear color space
	fragColor = sqrt(linearColor);	// approximate L^(1/gamma) w/ gamma = 2.2 by assuming gamma = 2
}
