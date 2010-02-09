varying vec2 Texcoord;

void main( void )
{
    gl_Position = ftransform();
    Texcoord    = gl_MultiTexCoord0.xy;
    Texcoord.x *= 0.3125;
    Texcoord.y *= 25.0;
}
