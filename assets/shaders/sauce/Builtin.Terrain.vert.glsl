#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in vec4 in_color;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec4 in_mat_weights;

struct DirectionalLight
{
    vec4 color;
    vec4 direction;
};

struct PointLight 
{
    vec4 color;
    vec4 position;
    // Usually 1, make sure denominator never gets smaller than 1
    float constant_f;
    // Reduces light intensity linearly
    float linear;
    // Makes the light fall off slower at longer distances.
    float quadratic;
    float padding;
};

const uint max_point_lights_count = 10;

layout(set = 0, binding = 0) uniform global_uniform_object
{
    mat4 projection;
    mat4 view;
    vec4 ambient_color;
    vec3 camera_position;
    int mode;
    DirectionalLight dir_light;
    PointLight p_lights[max_point_lights_count];
    uint p_lights_count;
} global_ubo;

layout(push_constant) uniform push_constants
{
    mat4 model;
} u_push_constants;

layout(location = 0) out int out_mode;
layout(location = 1) out struct dto
{
    vec4 ambient_color;
    vec3 normal;
    vec2 texcoord;
    vec3 camera_position;
    vec3 frag_position;
    vec4 color;
    vec3 tangent;
    vec4 mat_weights;
} out_dto;

void main()
{
    out_mode = global_ubo.mode;
    out_dto.frag_position = vec3(u_push_constants.model * vec4(in_position, 1.0));
    mat3 m3_model = mat3(u_push_constants.model);
	out_dto.normal = normalize(m3_model * in_normal);
	out_dto.tangent = normalize(m3_model * in_tangent.xyz);
    out_dto.texcoord = in_texcoord;
    out_dto.ambient_color = global_ubo.ambient_color;
    out_dto.camera_position = global_ubo.camera_position;

    out_dto.mat_weights = in_mat_weights;
    gl_Position = global_ubo.projection * global_ubo.view * u_push_constants.model * vec4(in_position, 1.0);
}