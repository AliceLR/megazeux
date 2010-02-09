uniform sampler2D baseMap;

varying vec2 Texcoord;

void main( void )
{
    vec4 tileinfo = texture2D( baseMap, vec2(Texcoord.x, (Texcoord.y+225.0)/256.0));
    if(texture2D(baseMap, vec2(frac(tileinfo.z*7.96875) + (frac(Texcoord.x*256.0)*0.03125), (floor(tileinfo.z*7.96875) + frac(Texcoord.y)) * 0.0546875)).x)
    {
      gl_FragColor = texture2D( baseMap, vec2(tileinfo.x, 224.0/256.0));
    }
    else
    {
      gl_FragColor = texture2D( baseMap, vec2(tileinfo.y, 224.0/256.0));
    }
}
