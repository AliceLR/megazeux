uniform sampler2D baseMap;

varying vec2 vTexcoord;

void main( void )
{
    vec4 tileinfo = texture2D( baseMap, vec2(vTexcoord.x, (vTexcoord.y+225.0)/256.0));
    if(texture2D(baseMap, vec2(fract(tileinfo.z*7.96875) + (fract(vTexcoord.x*256.0)*0.03125), (floor(tileinfo.z*7.9689) + fract(vTexcoord.y) + tileinfo.w*2048.0) * 0.0546875)).x > 0.5)
    {
      gl_FragColor = texture2D( baseMap, vec2(tileinfo.x, 224.0/256.0));
    }
    else
    {
      gl_FragColor = texture2D( baseMap, vec2(tileinfo.y, 224.0/256.0));
    }
}
