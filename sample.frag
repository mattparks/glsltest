#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(set = 0, binding = 1) uniform UboObject
{
	mat4 transform;
	float brightness;
} object;

layout(set = 0, binding = 2) uniform sampler2D samplerDiffuse;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inCameraPos;

layout(location = 0) out vec4 outColour;

const vec3 lightDirection = vec3(0.8f, 0.9f, 0.2f);

void main()
{
	vec3 diffuse = texture(samplerDiffuse, inUv).rgb;
	
	vec3 lightVector = -normalize(lightDirection);
	float brightness = max(dot(lightVector, normalize(inNormal)), 0.0);

	outColour = vec4((object.brightness + brightness) * diffuse, 1.0f);
}