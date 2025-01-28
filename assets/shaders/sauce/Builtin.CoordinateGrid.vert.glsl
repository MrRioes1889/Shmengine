#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in uint in_index;

layout(set = 0, binding = 0) uniform global_uniform_object
{
    mat4 projection;
    mat4 view;
    float near;
    float far;
} global_ubo;

// Data Transfer Object
layout(location = 1) out struct DTO 
{
	vec3 near_point;
    vec3 far_point;
} out_dto;


vec3 gridPlane[6] = vec3[](
    vec3(-1, 1, 0), vec3(-1, -1, 0), vec3(1, 1, 0),
    vec3(1, -1, 0), vec3(1, 1, 0), vec3(-1, -1, 0)
);

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 projection) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(projection);
    vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec3 p = gridPlane[in_index].xyz;
    out_dto.near_point = UnprojectPoint(p.x, p.y, 0.0, global_ubo.view, global_ubo.projection).xyz; // unprojecting on the near plane
    out_dto.far_point = UnprojectPoint(p.x, p.y, 1.0, global_ubo.view, global_ubo.projection).xyz; // unprojecting on the far plane
    gl_Position = vec4(p, 1.0); // using directly the clipped coordinates
}