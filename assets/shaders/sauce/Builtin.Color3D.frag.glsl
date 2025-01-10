#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec4 out_color;

// Data Transfer Object
layout(location = 1) in struct DTO 
{
	vec4 color;
} in_dto;

void main() {
    out_color = in_dto.color;
}