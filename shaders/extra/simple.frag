#ifndef GL_ES
#version 120
#endif

precision mediump float;

uniform sampler2D baseMap;

varying vec2 vTexcoord;

void main( void )
{
    gl_FragColor = texture2D( baseMap, vTexcoord );
}
