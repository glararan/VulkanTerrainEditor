#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 UV;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(pos, 1.0);

    outNormal = normal;
    outUV = UV;
}
