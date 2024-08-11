#ifndef UNPACK_ATTRIBUTES_H_INCLUDED
#define UNPACK_ATTRIBUTES_H_INCLUDED


vec3 DecodeNormal(uint a_data)
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

#endif // UNPACK_ATTRIBUTES_H_INCLUDED
