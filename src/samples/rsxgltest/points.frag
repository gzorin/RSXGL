#version 130
uniform sampler2D texture;

void
main(void)
{
  vec4 image = texture2D(texture,gl_PointCoord);
  gl_FragColor = vec4(image.rgb,1.0);
}
