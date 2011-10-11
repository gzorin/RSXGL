uniform sampler2D texture;

varying vec2 tc;
varying vec4 color;

void
main(void)
{
  gl_FragColor = texture2D(texture,tc) * vec4(color.rgb,1.0);
}
