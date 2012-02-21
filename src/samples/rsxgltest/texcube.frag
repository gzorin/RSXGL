#version 130
uniform sampler2D image, gradient;
uniform vec4 foo;

varying vec2 tc;

void
main(void)
{
  //gl_FragColor = vec4(tc.x,tc.y,0,1);
  gl_FragColor = texture2D(image,tc) * texture(gradient,tc);
}
