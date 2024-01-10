#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec2 screenCoords;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D renderResult;

void main()
{
	vec4 linearColor = texture(renderResult, screenCoords);	// output is in linear color space
	vec4 gammaSpaceColor = sqrt(linearColor);	// approximate L^(1/gamma) w/ gamma = 2.2 by assuming gamma = 2
	fragColor = gammaSpaceColor;
}
