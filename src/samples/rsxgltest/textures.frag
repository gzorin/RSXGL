uniform vec4 c = vec4(1,0,0,1);
uniform sampler2D texture;

varying vec2 tc;

void
main(void)
{
  gl_FragColor = texture2D(texture,tc);
  //gl_FragColor = vec4(tc.x,tc.y,0,1);
}
