#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec4 out_color;

layout(location = 1) in struct DTO 
{
	vec3 near_point;
    vec3 far_point;
} in_dto;

layout(set = 0, binding = 0) uniform global_uniform_object
{
    mat4 projection;
    mat4 view;
    float near;
    float far;
} global_ubo;

vec4 grid(vec3 fragPos3D, float scale, bool drawAxis) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec4 color = vec4(0.8, 0.8, 0.8, 1.0 - min(line, 1.0));
    // z axis
    if(fragPos3D.x > (-1.0 / scale) * minimumx && fragPos3D.x < (1.0 / scale) * minimumx)
        color.xyz = vec3(0.0, 0.0, 1.0);
    // x axis
    if(fragPos3D.z > (-1.0 / scale) * minimumz && fragPos3D.z < (1.0 / scale) * minimumz)
        color.xyz = vec3(1.0, 0.0, 0.0);
    return color;
}

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = global_ubo.projection * global_ubo.view * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

float computeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = global_ubo.projection * global_ubo.view * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * global_ubo.near * global_ubo.far) / (global_ubo.far + global_ubo.near - clip_space_depth * (global_ubo.far - global_ubo.near)); // get linear value between 0.01 and 100
    return linearDepth / global_ubo.far; // normalize
}

void main() {
    float t = -in_dto.near_point.y / (in_dto.far_point.y - in_dto.near_point.y);
    vec3 fragPos3D = in_dto.near_point + t * (in_dto.far_point - in_dto.near_point);

    gl_FragDepth = computeDepth(fragPos3D);

    float linearDepth = computeLinearDepth(fragPos3D);
    float fading = max(0, (0.5 - linearDepth));

    out_color = (grid(fragPos3D, 0.1, true) + grid(fragPos3D, 1, true)) * float(t > 0); // adding multiple resolution for the grid
    out_color.a *= fading;
}
