#version 110

/* Greyscale shader based on nearest.frag --Lachesis */

uniform sampler2D baseMap;

varying vec2 vTexcoord;

#define XS 1024.0
#define YS 512.0
#define AX 0.5/XS
#define AY 0.5/YS

void main( void )
{
  const vec4 weight = vec4(0.30, 0.59, 0.11, 0);

  vec2 tcbase = (floor(vTexcoord*vec2(XS, YS) + 0.5) + 0.5)/vec2(XS, YS);
  vec2 tcdiff = vTexcoord-tcbase;
  vec2 sdiff = sign(tcdiff);
  vec2 adiff = pow(abs(tcdiff)*vec2(XS, YS), vec2(2.0));
  vec4 color = texture2D(baseMap, tcbase + sdiff*adiff/vec2(XS, YS));

  float lum = dot( color, weight );

  gl_FragColor = vec4(lum, lum, lum, 1.0);
}