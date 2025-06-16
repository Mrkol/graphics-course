bool Cull(vec3 minP, vec3 maxP, mat4 projMat)
{
  vec3 minV;
  vec3 maxV;
  {
    vec4 proj = projMat * vec4(minP, 1);
    proj /= abs(proj.w);
    minV = proj.xyz;
    maxV = proj.xyz;
  }
  for (uint mask = 1; mask < 8u; ++mask)
  {
    vec3 point;
    for (uint i = 0; i < 3; ++i)
    {
      point[i] = ((mask & (1u << i)) != 0) ? maxP[i] : minP[i];
    }
    vec4 corner = projMat * vec4(point, 1);
    corner /= abs(corner.w);
    minV = min(minV, corner.xyz);
    maxV = max(maxV, corner.xyz);
  }
  return any(lessThan(maxV, vec3(-1, -1, 0))) || any(greaterThan(minV, vec3(1, 1, 1)));
}

// Converts a color from sRGB gamma to linear light gamma
vec3 toLinear(vec3 sRGB)
{
  bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
  vec3 higher = pow((sRGB + vec3(0.055)) / vec3(1.055), vec3(2.4));
  vec3 lower = sRGB / vec3(12.92);

  return mix(higher, lower, cutoff);
}

// Converts a color from linear light gamma to sRGB gamma
vec3 fromLinear(vec3 linearRGB)
{
  bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
  vec3 higher = vec3(1.055) * pow(linearRGB, vec3(1.0 / 2.4)) - vec3(0.055);
  vec3 lower = linearRGB * vec3(12.92);

  return mix(higher, lower, cutoff);
}


vec4 toLinear(vec4 sRGB)
{
  sRGB.rgb = toLinear(sRGB.rgb);
  return sRGB;
}

// Converts a color from linear light gamma to sRGB gamma
vec4 fromLinear(vec4 linearRGB)
{
  linearRGB.rgb = fromLinear(linearRGB.rgb);
  return linearRGB;
}
