#version 450
#include "Builtin.Skybox.common.glsl"

layout(location = 0) in vec3 in_position;

layout(location = 0) out vec3 tex_coord;

void main() {
	tex_coord = in_position;
	gl_Position = global_ubo.projection * global_ubo.view * vec4(in_position, 1.0);
} 