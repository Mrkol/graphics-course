#ifndef UNPACK_ATTRIBUTES_GLSL_INCLUDED
#define UNPACK_ATTRIBUTES_GLSL_INCLUDED

// NOTE: .glsl extension is used for helper files with shader code

vec3 decode_normal(uint a_data)
{
  const uint a_enc_x = (a_data  & 0x0000FFFFu);
  const uint a_enc_y = ((a_data & 0xFFFF0000u) >> 16);
  const float sign   = (a_enc_x & 0x0001u) != 0 ? -1.0f : 1.0f;

  const int usX = int(a_enc_x & 0x0000FFFEu);
  const int usY = int(a_enc_y & 0x0000FFFFu);

  const int sX  = (usX <= 32767) ? usX : usX - 65536;
  const int sY  = (usY <= 32767) ? usY : usY - 65536;

  const float x = sX*(1.0f / 32767.0f);
  const float y = sY*(1.0f / 32767.0f);
  const float z = sign*sqrt(max(1.0f - x*x - y*y, 0.0f));

  return vec3(x, y, z);
}

vec3 decode_baked_normal(uint a_data)
{
  const uint a_enc_x = ((a_data & 0x000000FFu) >> 0);
  const uint a_enc_y = ((a_data & 0x0000FF00u) >> 8);
  const uint a_enc_z = ((a_data & 0x00FF0000u) >> 16);

  ivec3 tmp = ivec3(a_enc_x, a_enc_y, a_enc_z);
  vec3 decoded_vec = vec3((tmp + 128) % 256 - 128);
  decoded_vec = max((decoded_vec) / 127.0, -1.0);

  return decoded_vec;
}

#endif // UNPACK_ATTRIBUTES_GLSL_INCLUDED
