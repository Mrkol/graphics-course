#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout (location = 0 ) in VS_OUT
{
  vec3 texCoord;
} vIn;

layout(location = 0) out vec4 out_fragColor;

layout(binding = 0) uniform samplerCube skybox;

layout(push_constant) uniform pc {
    mat4 mProjView;
} params;

void main() {
    out_fragColor = texture(skybox, normalize(vIn.texCoord));
}
