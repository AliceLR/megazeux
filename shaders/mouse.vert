attribute vec2 Position;

void main(void)
{
    gl_Position = vec4(Position.x, Position.y, 0.0, 1.0);
}
