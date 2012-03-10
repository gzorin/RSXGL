#version 130
uniform sampler2D texture;

varying vec2 tc;
varying vec4 color;

void
main(void)
{
  //gl_FragColor = texture2D(texture,tc) * vec4(color.rgb,1.0);

  //gl_FragData[0] = vec4(1,0,0,1) * vec4(color.rgb,1.0);
  //gl_FragData[1] = vec4(0,1,0,1) * vec4(color.rgb,1.0);

  gl_FragData[0] = vec4(1,0,1,1);
  gl_FragData[1] = vec4(1,1,0,1);

  //gl_FragColor = vec4(1,0,0,1);
}
