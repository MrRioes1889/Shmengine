#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) flat in int in_mode;
layout(location = 1) in struct dto
{
    vec4 ambient_color;
    vec3 normal;
    vec2 texcoord;
    vec3 camera_position;
    vec3 frag_position;
    vec4 color;
    vec3 tangent;
} in_dto;

layout(location = 0) out vec4 out_color;

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

struct MaterialPhongProperties 
{
    vec4 diffuse_color; 
    vec3 padding;
    float shininess;
};

layout(std430, set = 1, binding = 0) uniform instance_uniform_object
{
    MaterialPhongProperties properties;
} instance_ubo;

const int SAMP_DIFFUSE = 0;
const int SAMP_SPECULAR = 1;
const int SAMP_NORMAL = 2;
layout(set = 1, binding = 1) uniform sampler2D samplers[3];

mat3 TBN;

vec4 calc_dir_lighting(DirectionalLight light, vec3 normal, vec3 view_direction)
{
    float diffuse_factor = max(dot(normal, -light.direction.xyz), 0.0);

    vec3 half_direction = normalize(view_direction - light.direction.xyz);
    float specular_factor = pow(max(dot(half_direction, normal), 0.0), instance_ubo.properties.shininess);

    vec4 diff_samp = texture(samplers[SAMP_DIFFUSE], in_dto.texcoord);
    vec4 ambient = vec4(vec3(in_dto.ambient_color * instance_ubo.properties.diffuse_color), diff_samp.a);
    vec4 diffuse = vec4(vec3(light.color * diffuse_factor), diff_samp.a);
    vec4 specular = vec4(vec3(light.color * specular_factor), diff_samp.a);

    if (in_mode == 0)
    {
        diffuse *= diff_samp;
        ambient *= diff_samp;
        specular *= vec4(texture(samplers[SAMP_SPECULAR], in_dto.texcoord).rgb, diffuse.a);
    } 

    return (ambient + diffuse + specular);
}

vec4 calc_point_lighting(PointLight light, vec3 normal, vec3 frag_position, vec3 view_direction)
{
    vec3 light_direction = normalize(light.position.xyz - frag_position);
    float diff = max(dot(normal, light_direction), 0.0);

    vec3 reflect_direction = reflect(-light_direction, normal);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0), instance_ubo.properties.shininess);

    // Calculate attenuation, or light falloff over distance.
    float distance = length(light.position.xyz - frag_position);
    float attenuation = 1.0 / (light.constant_f + light.linear * distance + light.quadratic * (distance * distance));

    vec4 ambient = in_dto.ambient_color;
    vec4 diffuse = light.color * diff;
    vec4 specular = light.color * spec;

    if (in_mode == 0) 
    {
        vec4 diff_samp = texture(samplers[SAMP_DIFFUSE], in_dto.texcoord);
        diffuse *= diff_samp;
        ambient *= diff_samp;
        specular *= vec4(texture(samplers[SAMP_SPECULAR], in_dto.texcoord).rgb, diffuse.a);
    }

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

void main()
{
    vec3 normal = in_dto.normal;
    vec3 tangent = in_dto.tangent;
    tangent = (tangent - dot(tangent, normal) *  normal);
    vec3 bitangent = cross(in_dto.normal, in_dto.tangent);
    TBN = mat3(tangent, bitangent, normal);

    // Update the normal to use a sample from the normal map.
    vec3 localNormal = 2.0 * texture(samplers[SAMP_NORMAL], in_dto.texcoord).rgb - 1.0;
    normal = normalize(TBN * localNormal);

    if(in_mode == 0 || in_mode == 1) 
    {
        vec3 view_direction = normalize(in_dto.camera_position - in_dto.frag_position);

        out_color = calc_dir_lighting(global_ubo.dir_light, normal, view_direction);

        for (uint i = 0; i < global_ubo.p_lights_count; i++)
        {
            out_color += calc_point_lighting(global_ubo.p_lights[i], normal, in_dto.frag_position, view_direction);
        }
        
    }
    else if (in_mode == 2)
    {
        out_color = vec4(abs(normal), 1.0);
    }

    
}