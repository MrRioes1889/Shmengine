#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in vec4 in_color;
layout(location = 4) in vec4 in_tangent;

struct directional_light
{
    vec4 color;
    vec4 direction;
};

struct point_light {
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
    mat4 model;
    vec4 ambient_color;
    vec3 camera_position;
    int mode;
    vec4 diffuse_color;
    directional_light dir_light;
    point_light p_lights[max_point_lights_count];
    int p_lights_count;
    float shininess;
} global_ubo;

layout(location = 0) out int out_mode;
layout(location = 1) out struct dto
{
    vec4 ambient_color;
    vec3 normal;
    vec2 texcoord;
    vec3 camera_position;
    vec3 frag_position;
    vec4 color;
    vec4 tangent;
} out_dto;

void main()
{
    out_mode = global_ubo.mode;
    out_dto.frag_position = vec3(global_ubo.model * vec4(in_position, 1.0));
    mat3 m3_model = mat3(global_ubo.model);
	out_dto.normal = normalize(m3_model * in_normal);
	out_dto.tangent.xyz = normalize(m3_model * in_tangent.xyz);
    out_dto.texcoord = in_texcoord;
    out_dto.ambient_color = global_ubo.ambient_color;
    out_dto.camera_position = global_ubo.camera_position;

    gl_Position = global_ubo.projection * global_ubo.view * global_ubo.model * vec4(in_position, 1.0);
}