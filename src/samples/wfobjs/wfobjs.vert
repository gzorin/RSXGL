attribute vec3 vertex;
attribute vec3 normal;
attribute vec2 uv;

uniform mat4 ProjMatrix;
uniform mat4 TransMatrix;

void
main(void)
{
  vec4 v = TransMatrix * vec4(vertex.x,vertex.y,vertex.z,1);
  gl_Position = ProjMatrix * v;
}
