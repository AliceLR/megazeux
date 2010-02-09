uniform sampler2D baseMap;

varying vec2 Texcoord;

void main( void )
{
    gl_FragColor = texture2D( baseMap, vec2(Texcoord.x, Texcoord.y))*2 - texture2D( baseMap, vec2(Texcoord.x - 0.0005, Texcoord.y + 0.0007))*2 + texture2D( baseMap, vec2(Texcoord.x + 0.0006, Texcoord.y - 0.001));
    
}