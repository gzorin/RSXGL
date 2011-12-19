attribute vec3 position;

uniform mat4 ProjMatrix;
uniform mat4 TransMatrix;
uniform vec3 c;

varying vec3 color;

void
main(void)
{
  gl_Position = ProjMatrix * (TransMatrix * vec4(position,1));
  color = c;
}
