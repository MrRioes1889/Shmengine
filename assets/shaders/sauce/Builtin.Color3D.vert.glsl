#version 450
#include "Builtin.Color3D.common.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;

layout(location = 1) out Dto out_dto;

void main()
{
	out_dto.color = in_color;
	gl_Position = global_ubo.projection * global_ubo.view * local_ubo.model * vec4(in_position, 1.0);
}