attribute vec2 position, tc;

uniform mat4 ProjMatrix;
uniform vec2 xy;

varying vec2 uv;

void
main(void)
{
  gl_Position = ProjMatrix * vec4(position + xy,0,1);
  uv = tc;
}
