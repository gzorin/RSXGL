#version 130
varying vec3 c;

void
main(void)
{
  gl_FragColor = vec4(c,1);
}
