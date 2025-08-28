#version 450
#include "Builtin.MaterialPhong.common.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in vec4 in_color;
layout(location = 4) in vec3 in_tangent;

layout(location = 0) out int out_mode;
layout(location = 1) out Dto out_dto;

void main()
{
    out_mode = global_ubo.mode;
    out_dto.frag_position = vec3(u_push_constants.model * vec4(in_position, 1.0));
    mat3 m3_model = mat3(u_push_constants.model);
	out_dto.normal = normalize(m3_model * in_normal);
	out_dto.tangent = normalize(m3_model * in_tangent);
    out_dto.texcoord = in_texcoord;
    out_dto.ambient_color = global_ubo.ambient_color;
    out_dto.camera_position = global_ubo.camera_position;

    gl_Position = global_ubo.projection * global_ubo.view * u_push_constants.model * vec4(in_position, 1.0);
}