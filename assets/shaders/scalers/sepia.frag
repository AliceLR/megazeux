#version 110

/* Sepia shader based on simple.frag --Lachesis */

uniform sampler2D baseMap;

varying vec2 vTexcoord;

void main( void )
{
  const vec4 r_weight = vec4(0.393, 0.769, 0.189, 0);
  const vec4 g_weight = vec4(0.349, 0.686, 0.168, 0);
  const vec4 b_weight = vec4(0.272, 0.534, 0.131, 0);

  vec4 color = texture2D( baseMap, vTexcoord );

  gl_FragColor = vec4(
   min(1.0, dot(color, r_weight)),
   min(1.0, dot(color, g_weight)),
   min(1.0, dot(color, b_weight)),
   1.0
  );
}
