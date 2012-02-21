#version 130
attribute vec3 vertex;
attribute vec2 uv;

uniform mat4 ProjMatrix;
uniform mat4 TransMatrix;

varying vec2 tc;

void
main(void)
{
  tc = uv;

  vec4 v = TransMatrix * vec4(vertex.x,vertex.y,vertex.z,1);
  gl_Position = ProjMatrix * v;
}
