uniform sampler2D baseMap;

varying vec2 vTexcoord;

float fract_(float v)
{
  return clamp(fract(v + 0.001) - 0.001, 0.000, 0.999);
}

float floor_(float v)
{
  return floor(v + 0.001);
}

void main( void )
{
    vec4 tileinfo = texture2D( baseMap, vec2(vTexcoord.x / 2.0, (vTexcoord.y+901.0)/1024.0));

    if(
      texture2D(baseMap, vec2(
        fract_((floor_(tileinfo.z * 63.75) + tileinfo.w * 255.0 * 64.0) / 64.0) + fract_(vTexcoord.x * 256.0) / 64.0
        ,
        floor_((floor_(tileinfo.z * 63.75) + tileinfo.w * 255.0 * 64.0) / 64.0) * 14.0 / 1024.0 + fract_(vTexcoord.y) * 14.0 / 1024.0
       )
      ).x > 0.5)
    {
      gl_FragColor = texture2D( baseMap, vec2(tileinfo.x * 0.5 + fract_(tileinfo.y * 127.501), 896.0/1024.0));
    }
    else
    {
      gl_FragColor = texture2D( baseMap, vec2(floor_(tileinfo.y * 127.5) / 512.0 + fract_(tileinfo.z * 63.751), 896.0/1024.0));
    }
}