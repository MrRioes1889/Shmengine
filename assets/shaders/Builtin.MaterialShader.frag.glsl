#version 450

layout(location = 0) flat in int in_mode;
layout(location = 1) in struct dto
{
    vec4 ambient_color;
    vec3 normal;
    vec2 texcoord;
    vec3 camera_position;
    vec3 frag_position;
    vec4 color;
    vec4 tangent;
} in_dto;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform local_uniform_object
{
    vec4 diffuse_color;
    float shininess;
} object_ubo;

const int SAMP_DIFFUSE = 0;
const int SAMP_SPECULAR = 1;
const int SAMP_NORMAL = 2;
layout(set = 1, binding = 1) uniform sampler2D samplers[3];

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

mat3 TBN;

vec4 calc_color(vec3 normal)
{
    vec3 view_direction = normalize(in_dto.camera_position - in_dto.frag_position);

    float diffuse_factor = max(dot(normal, -dir_light.direction), 0.0);

    vec3 half_direction = normalize(view_direction - dir_light.direction);
    float specular_factor = pow(max(dot(half_direction, normal), 0.0), object_ubo.shininess);

    vec4 diff_samp = texture(samplers[SAMP_DIFFUSE], in_dto.texcoord);
    vec4 ambient = vec4(vec3(in_dto.ambient_color * object_ubo.diffuse_color), diff_samp.a);
    vec4 diffuse = vec4(vec3(dir_light.color * diffuse_factor), diff_samp.a);
    vec4 specular = vec4(vec3(dir_light.color * specular_factor), diff_samp.a);

    diffuse *= diff_samp;
    ambient *= diff_samp;
    specular *= vec4(texture(samplers[SAMP_SPECULAR], in_dto.texcoord).rgb, diffuse.a);

    return (ambient + diffuse + specular);
}

void main()
{
    vec3 normal = in_dto.normal;
    vec3 tangent = in_dto.tangent.xyz;
    tangent = (tangent - dot(tangent, normal) *  normal);
    vec3 bitangent = cross(in_dto.normal, in_dto.tangent.xyz) * in_dto.tangent.w;
    TBN = mat3(tangent, bitangent, normal);

    // Update the normal to use a sample from the normal map.
    vec3 localNormal = 2.0 * texture(samplers[SAMP_NORMAL], in_dto.texcoord).rgb - 1.0;
    normal = normalize(TBN * localNormal);

    out_color = calc_color(normal);
}