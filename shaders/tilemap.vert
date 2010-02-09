attribute vec2 Position;
attribute vec2 Texcoord;

varying vec2 vTexcoord;

void main(void)
{
    gl_Position = vec4(Position.x, Position.y, 0.0, 1.0);
    vTexcoord.x = Texcoord.x * 0.3125;
    vTexcoord.y = Texcoord.y * 25.0;
}
