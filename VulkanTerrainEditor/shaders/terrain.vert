#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 pos;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(set = 0, binding = 0) uniform UBO
{
    mat4 mvp;
} ubo;

void main()
{
    gl_Position = ubo.mvp * vec4(pos, 1.0);
}
