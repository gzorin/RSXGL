#version 130
attribute vec3 position;
attribute vec3 color;

uniform mat4 ProjMatrix;
uniform mat4 TransMatrix;

void
main(void)
{
  gl_Position = ProjMatrix * (TransMatrix * vec4(position,1));

  //gl_Position = ProjMatrix * (TransMatrix * vec4(color,1));
  //dumb = position;
}
