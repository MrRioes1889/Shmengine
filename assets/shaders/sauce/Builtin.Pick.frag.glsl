#version 450
#include "Builtin.Pick.common.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in Dto in_dto; 

void main()
{
    out_color = in_dto.id_color;  
}