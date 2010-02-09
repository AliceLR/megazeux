#ifndef GL_ES
#version 120
#endif

precision mediump float;

varying vec3 vColor;

void main(void)
{
  gl_FragColor = vec4(vColor.x, vColor.y, vColor.z, 1.0);
}
