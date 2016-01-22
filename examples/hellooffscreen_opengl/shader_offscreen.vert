attribute vec4 position;
attribute vec4 color;

varying vec4 vColor;

uniform mat4 projection;
uniform mat4 modelview;

void main()
{
    vColor = color;
    gl_Position = projection * modelview * position;
}
