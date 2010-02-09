uniform sampler2D baseMap;

varying vec2 vTexcoord;

void main( void )
{
    gl_FragColor = texture2D( baseMap, vec2(vTexcoord.x, vTexcoord.y))*2 - texture2D( baseMap, vec2(vTexcoord.x - 0.0005, vTexcoord.y + 0.0007))*2 + texture2D( baseMap, vec2(vTexcoord.x + 0.0006, vTexcoord.y - 0.001));
    
}
