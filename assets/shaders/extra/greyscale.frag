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
  vec4 color = texture2D( baseMap, vec2(floor(vTexcoord.x*XS)/XS+AX, floor(vTexcoord.y*YS)/YS+AY) );
  float lum = dot( color, weight );

  gl_FragColor = vec4(lum, lum, lum, 1.0);
}