#version 450

layout (vertices=4) out;

layout(location = 0) in vec2 TexCoord[];

layout(location = 0) out vec2 TextureCoord[];

layout(push_constant) uniform params_t
{
  vec2 base; 
  vec2 extent;
  mat4 mProjView;
  vec3 camPos;
  int degree;
} params;

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    TextureCoord[gl_InvocationID] = TexCoord[gl_InvocationID];
    vec3 camPos = params.camPos;
    vec4 dists = vec4(
        length(gl_in[0].gl_Position.xyz - camPos),
        length(gl_in[1].gl_Position.xyz - camPos),
        length(gl_in[2].gl_Position.xyz - camPos),
        length(gl_in[3].gl_Position.xyz - camPos)
    );

    float tscoef = 2048. * 128;

    if (gl_InvocationID == 0)
    {
        gl_TessLevelOuter[0] = tscoef / pow(dists.x + dists.y, 2.5);
        gl_TessLevelOuter[1] = tscoef / pow(dists.y + dists.z, 2.5);
        gl_TessLevelOuter[2] = tscoef / pow(dists.z + dists.w, 2.5);
        gl_TessLevelOuter[3] = tscoef / pow(dists.w + dists.x, 2.5);

        gl_TessLevelInner[0] = gl_TessLevelOuter[0] + gl_TessLevelOuter[2];
        gl_TessLevelInner[1] = gl_TessLevelOuter[1] + gl_TessLevelOuter[3];
    }
}