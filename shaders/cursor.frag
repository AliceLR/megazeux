#version 120

precision mediump float;

varying vec3 vColor;

void main(void)
{
  gl_FragColor = vec4(vColor.x, vColor.y, vColor.z, 1.0);
}
