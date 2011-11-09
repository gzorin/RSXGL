attribute vec3 position;
attribute vec3 color;

uniform float rsxgl_InstanceID;
uniform mat4 ProjMatrix;
uniform mat4 TransMatrix;
uniform sampler1D texture;

varying vec3 c;

void
main(void)
{
  float x = (rsxgl_InstanceID / 100.0) * 50.0 - 25.0;
  vec3 p = position + vec3(x,x,x);
  gl_Position = ProjMatrix * (TransMatrix * vec4(p,1));
  c = texture1D(texture,rsxgl_InstanceID / 100.0).rgb;
}
