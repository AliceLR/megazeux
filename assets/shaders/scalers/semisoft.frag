#version 110
uniform sampler2D baseMap;

varying vec2 vTexcoord;
#define XS 1024.0
#define YS 512.0
#define AX 0.5/XS
#define AY 0.5/YS
void main( void )
{
    vec2 tcbase = (floor(vTexcoord*vec2(XS, YS) + 0.5) + 0.5)/vec2(XS, YS);
    vec2 tcdiff = vTexcoord-tcbase;
    vec2 sdiff = sign(tcdiff);
    vec2 adiff = pow(abs(tcdiff)*vec2(XS, YS), vec2(3.0));
    gl_FragColor = texture2D(baseMap, tcbase + sdiff*adiff/vec2(XS, YS));
}