#extension GL_EXT_scalar_block_layout : enable

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

struct Dto
{
    vec4 ambient_color;
    vec3 normal;
    vec2 texcoord;
    vec3 camera_position;
    vec3 frag_position;
    vec4 color;
    vec3 tangent;
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

layout(push_constant) uniform push_constants
{
    mat4 model;
} u_push_constants;
