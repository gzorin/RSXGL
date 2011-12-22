attribute vec2 position;

uniform mat4 ProjMatrix;

varying vec2 uv;

void
main(void)
{
  gl_Position = ProjMatrix * vec4(position,0.5,1);
  uv = position;
}
