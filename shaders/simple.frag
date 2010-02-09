uniform sampler2D baseMap;

varying vec2 Texcoord;

void main( void )
{
    gl_FragColor = texture2D( baseMap, Texcoord );
}
