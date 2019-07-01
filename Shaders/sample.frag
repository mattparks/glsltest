#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (constant_id = 0) const bool LIT = true;

layout (binding = 1) uniform UniformObject
{
	vec3 lights[9];
	mat4 transform;
	float brightness;
} object;

layout (push_constant) uniform PushLights
{
	float effect[9];
} lights;

layout (binding = 2) uniform sampler2D samplerDiffuse;

layout (location = 0) in vec2 inUv;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inCameraPos;

layout (location = 0) out vec4 outColour;

void main()
{
	vec3 diffuse = texture(samplerDiffuse, inUv).rgb;

	float brightness = 0.0f;

	if (LIT)
	{
		for (int i = 0; i < 9; i++)
		{
			vec3 lightVector = -normalize(object.lights[i]);
			brightness += lights.effect[i] * max(dot(lightVector, inNormal), 0.0);
		}
	}
	else
	{
		brightness = 1.0f;
	}

	outColour = vec4((object.brightness + brightness) * diffuse, 1.0f);
}