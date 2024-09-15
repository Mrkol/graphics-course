#ifndef LIGHT_PASS_VERT_IMPL_H
#define LIGHT_PASS_VERT_IMPL_H

#ifndef LIGHT_PARAMS_TYPE
#error "LIGHT_PARAMS_TYPE is not defined"
#endif

#ifndef LIGHT_GET_VERTEX
#error "LIGHT_GET_VERTEX is not defined"
#endif


layout(location = 0) out VS_OUT
{
  noperspective vec2 texCoord;
  LIGHT_PARAMS_TYPE light;
}
vOut;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
}
params;

layout(binding = 0, set = 0) buffer lightsData_t
{
  LIGHT_PARAMS_TYPE lights[];
}
lightsData;

void main()
{
  LIGHT_PARAMS_TYPE light = lightsData.lights[gl_InstanceIndex];

  vec3 position = LIGHT_GET_VERTEX(light);

#ifdef LIGHT_VERTEX_IN_WORLD_SPACE
  vec4 clipPositionH = params.mProjView * vec4(position, 1.0);
#else
  vec4 clipPositionH = vec4(position, 1.0);
#endif

  vOut.texCoord = (clipPositionH.xy / clipPositionH.w + 1.0) / 2.0;
  vOut.light = light;

  gl_Position = clipPositionH;
}

#endif
