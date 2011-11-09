attribute vec3 position;
attribute vec3 color;

uniform float rsxgl_InstanceID, ncubes;
uniform mat4 ProjMatrix;
uniform mat4 TransMatrix;
uniform sampler1D texture;

varying vec3 c;

void
main(void)
{
  float _nvecs = ncubes * 4.0;
  float _delta = 1.0 / _nvecs;
  float _delta0 = 0.0 * _delta;
  float _delta1 = 1.0 * _delta;
  float _delta2 = 2.0 * _delta;
  float _delta3 = 3.0 * _delta;

  float i = (rsxgl_InstanceID * 4.0) / _nvecs;

  mat4 m = mat4(texture1D(texture,i + _delta0),
  		texture1D(texture,i + _delta1),
  		texture1D(texture,i + _delta2),
  		texture1D(texture,i + _delta3));

  gl_Position = ProjMatrix * (TransMatrix * m * vec4(position,1));

  c = vec3(0.5,0.5,0.5);
}
