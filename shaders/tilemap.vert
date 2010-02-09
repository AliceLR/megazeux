#ifndef GL_ES
#version 120
#endif

precision mediump float;

attribute vec2 Position;
attribute vec2 Texcoord;

varying vec2 vTexcoord;

void main(void)
{
    gl_Position = vec4(Position.x, Position.y, 0.0, 1.0);
    vTexcoord = Texcoord;
}
