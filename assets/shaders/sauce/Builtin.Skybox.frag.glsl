#version 450
#include "Builtin.Skybox.common.glsl"

layout(location = 0) in vec3 tex_coord;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = texture(samplers[SAMP_DIFFUSE], tex_coord);
} 