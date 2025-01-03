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
    vec3 tangent;
    vec4 mat_weights;
} in_dto;

const int max_terrain_materials_count = 4;

layout(location = 0) out vec4 out_color;

// const int SAMP_DIFFUSE = 0;
// const int SAMP_SPECULAR = 1;
// const int SAMP_NORMAL = 2;
// layout(set = 1, binding = 1) uniform sampler2D samplers[3];

struct DirectionalLight
{
    vec4 color;
    vec4 direction;
};

struct PointLight {
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
} global_ubo;

struct MaterialPhongProperties {
    vec4 diffuse_color;
    vec3 padding;
    float shininess;
};

struct MaterialTerrainProperties {
    MaterialPhongProperties materials[max_terrain_materials_count];   
    vec3 padding;
    uint materials_count;
};

layout(set = 1, binding = 0) uniform instance_uniform_object {
    MaterialTerrainProperties properties;
    PointLight p_lights[max_point_lights_count];
    uint p_lights_count;
} instance_ubo;

const int samp_diffuse_index = 0;
const int samp_specular_index = 1;
const int samp_normal_index = 2;

layout(set = 1, binding = 1) uniform sampler2D samplers[3 * max_terrain_materials_count];

mat3 TBN;

vec4 calc_dir_lighting(DirectionalLight light, vec3 normal, vec3 view_direction, vec4 diff_samp, vec4 spec_samp, MaterialPhongProperties mat_properties)
{
    float diffuse_factor = max(dot(normal, -light.direction.xyz), 0.0);

    vec3 half_direction = normalize(view_direction - light.direction.xyz);
    float specular_factor = pow(max(dot(half_direction, normal), 0.0), mat_properties.shininess);

    vec4 ambient = vec4(vec3(in_dto.ambient_color * mat_properties.diffuse_color), diff_samp.a);
    vec4 diffuse = vec4(vec3(light.color * diffuse_factor), diff_samp.a);
    vec4 specular = vec4(vec3(light.color * specular_factor), diff_samp.a);

    if (in_mode == 0)
    {
        diffuse *= diff_samp;
        ambient *= diff_samp;
        specular *= spec_samp;
    } 

    return (ambient + diffuse + specular);
}

vec4 calc_point_lighting(PointLight light, vec3 normal, vec3 frag_position, vec3 view_direction, vec4 diff_samp, vec4 spec_samp, MaterialPhongProperties mat_properties)
{
    vec3 light_direction = normalize(light.position.xyz - frag_position);
    float diff = max(dot(normal, light_direction), 0.0);

    vec3 reflect_direction = reflect(-light_direction, normal);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0), mat_properties.shininess);

    // Calculate attenuation, or light falloff over distance.
    float distance = length(light.position.xyz - frag_position);
    float attenuation = 1.0 / (light.constant_f + light.linear * distance + light.quadratic * (distance * distance));

    vec4 ambient = in_dto.ambient_color;
    vec4 diffuse = light.color * diff;
    vec4 specular = light.color * spec;

    if (in_mode == 0) 
    {
        vec4 diff_samp = vec4(1.0);
        diffuse *= diff_samp;
        ambient *= diff_samp;
        specular *= spec_samp;
    }

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

void main()
{
    vec3 normal = in_dto.normal;
    vec3 tangent = in_dto.tangent.xyz;
    tangent = (tangent - dot(tangent, normal) *  normal);
    vec3 bitangent = cross(in_dto.normal, in_dto.tangent.xyz);
    TBN = mat3(tangent, bitangent, normal);

    // Update the normal to use a sample from the normal map.
    vec3 normals[max_terrain_materials_count];
    vec4 diffuses[max_terrain_materials_count];
    vec4 specs[max_terrain_materials_count];

    MaterialPhongProperties mat;
    mat.diffuse_color = vec4(0);
    mat.shininess = 1.0;

    // Manually sample the first material.
    // TODO: This mix isn't quite right.
    vec4 diff = vec4(1);
    vec4 spec = vec4(0.5);
    vec3 norm = vec3(0, 0, 1);
    for(int m = 0; m < instance_ubo.properties.materials_count; ++m) {
        normals[m] = normalize(TBN * (2.0 * texture(samplers[(m * 3) + samp_normal_index], in_dto.texcoord).rgb - 1.0));
        diffuses[m] = texture(samplers[(m * 3) + samp_diffuse_index], in_dto.texcoord);
        specs[m] = texture(samplers[(m * 3) + samp_specular_index], in_dto.texcoord);

        norm = mix(norm, normals[m], in_dto.mat_weights[m]);
        diff = mix(diff, diffuses[m], in_dto.mat_weights[m]);
        spec = mix(spec, specs[m], in_dto.mat_weights[m]);

        mat.diffuse_color = mix(mat.diffuse_color, instance_ubo.properties.materials[m].diffuse_color, in_dto.mat_weights[m]);
        mat.shininess = mix(mat.shininess, instance_ubo.properties.materials[m].shininess, in_dto.mat_weights[m]);
    }

    if(in_mode == 0 || in_mode == 1) 
    {
        vec3 view_direction = normalize(in_dto.camera_position - in_dto.frag_position);

        out_color = calc_dir_lighting(global_ubo.dir_light, norm, view_direction, diff, spec, mat);

        for (uint i = 0; i < instance_ubo.p_lights_count; i++)
        {
            out_color += calc_point_lighting(instance_ubo.p_lights[i], norm, in_dto.frag_position, view_direction, diff, spec, mat);
        }
        
    }
    else if (in_mode == 2)
    {
        out_color = vec4(abs(normal), 1.0);
    }

    
}