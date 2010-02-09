uniform sampler2D baseMap;

varying vec2 vTexcoord;

void main( void )
{
    vec4 tileinfo = texture2D( baseMap, vec2(vTexcoord.x, (vTexcoord.y+225.0)/256.0));
    bool b1 = (texture2D(baseMap, vec2(tileinfo.z*7.96875 + (mod(vTexcoord.x - mod(vTexcoord.x, 0.0009765625), 0.00390625)*8.0), (floor(tileinfo.z*7.9689) + mod(vTexcoord.y, 1.0) + tileinfo.w*2048.0) * 0.0546875)).x > 0.5);
    bool b2 = (texture2D(baseMap, vec2(0.00390625 + tileinfo.z*7.96875 + (mod(vTexcoord.x - mod(vTexcoord.x, 0.0009765625), 0.00390625)*8.0), (floor(tileinfo.z*7.9689) + mod(vTexcoord.y, 1.0) + tileinfo.w*2048.0) * 0.0546875)).x > 0.5);
 
    if(b2)
    {
        if(b1)
        {
            gl_FragColor = texture2D( baseMap, vec2(mod(tileinfo.x, 16.0627/256.0) * 16.9999, 224.0/256.0));
        }
        else
        {
            gl_FragColor = texture2D( baseMap, vec2(mod(tileinfo.x, 16.0627/256.0) * 0.9999 + mod(tileinfo.y, 16.0627/256.0) * 15.9999, 224.0/256.0));
        }
    }
    else
    {
        if(b1)
        {
            gl_FragColor = texture2D( baseMap, vec2(mod(tileinfo.y, 16.0627/256.0) * 0.9999 + mod(tileinfo.x, 16.0627/256.0) * 15.9999, 224.0/256.0));
        }
        else
        {
            gl_FragColor = texture2D( baseMap, vec2(mod(tileinfo.y, 16.0627/256.0) * 16.9999, 224.0/256.0));
        }
    }
 
}
