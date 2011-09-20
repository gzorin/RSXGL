attribute vec4 vertex;
attribute vec2 texcoord;

uniform mat4 ProjMatrix;
uniform mat4 TransMatrix;

varying vec2 tc;

void
main(void)
{
  vec4 v = TransMatrix * vertex;
  gl_Position = ProjMatrix * v;
  tc = texcoord;
}
