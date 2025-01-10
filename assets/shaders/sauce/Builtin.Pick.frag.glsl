#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform instance_uniform_object
{
    vec3 id_color;
} instance_ubo;

void main()
{
    out_color = vec4(instance_ubo.id_color, 1.0);  
}