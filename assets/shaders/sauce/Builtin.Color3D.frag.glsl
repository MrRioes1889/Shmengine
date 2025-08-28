#version 450
#include "Builtin.Color3D.common.glsl"

layout(location = 1) in Dto in_dto; 

layout(location = 0) out vec4 out_color;

void main() 
{
    out_color = in_dto.color;
}