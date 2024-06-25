#version 450

layout(location = 0) flat in int in_mode;
layout(location = 1) in struct dto
{
    vec4 ambient_color;
    vec3 normal;
    vec2 texcoord;
} in_dto;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform local_uniform_object
{
    vec4 diffuse_color;
} object_ubo;

layout(set = 1, binding = 1) uniform sampler2D diffuse_sampler;

struct directional_light
{
    vec3 direction;
    vec4 color;
};

directional_light dir_light =
{
    vec3(-0.57735, -0.57735, -0.57735),
    vec4(0.8, 0.8, 0.8, 1.0f)
};

vec4 calc_color()
{
    float diffuse_factor = max(dot(in_dto.normal, -dir_light.direction), 0.0);

    vec4 diff_samp = texture(diffuse_sampler, in_dto.texcoord);
    vec4 ambient = vec4(vec3(in_dto.ambient_color * object_ubo.diffuse_color), diff_samp.a);
    vec4 diffuse = vec4(vec3(dir_light.color * diffuse_factor), diff_samp.a);

    diffuse *= diff_samp;
    ambient *= diff_samp;

    return (ambient + diffuse);
}

void main()
{
    out_color = calc_color();
}