attribute vec4 position;
attribute vec2 texcoord;

varying vec2 vTexCoord;

uniform mat4 projection;
uniform mat4 modelview;

void main()
{
    vTexCoord = texcoord;
    gl_Position = projection * modelview * position;
}
