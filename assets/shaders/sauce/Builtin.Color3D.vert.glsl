#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;

layout(set = 0, binding = 0) uniform GlobalUBO 
{
    mat4 projection;
	mat4 view;
} global_ubo;

layout(push_constant) uniform PushConstants 
{
	// Only guaranteed a total of 128 bytes.
	mat4 model; // 64 bytes
} local_ubo;


// Data Transfer Object
layout(location = 1) out struct DTO 
{
	vec4 color;
} out_dto;

void main()
{
	out_dto.color = in_color;
	gl_Position = global_ubo.projection * global_ubo.view * local_ubo.model * vec4(in_position, 1.0);
}