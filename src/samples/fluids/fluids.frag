uniform sampler2D texture;

varying vec2 uv;

void
main(void)
{
  //gl_FragColor = vec4(uv,0,1);
  gl_FragColor = texture2D(texture,uv) * vec4(uv,0,1);
}
