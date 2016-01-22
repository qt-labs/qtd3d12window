varying highp vec2 vTexCoord;

uniform sampler2D sampler;

void main()
{
    gl_FragColor = vec4(texture2D(sampler, vTexCoord).rgb, 1.0);
}
