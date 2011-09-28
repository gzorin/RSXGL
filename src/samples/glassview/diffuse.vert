attribute vec3 vertex;
attribute vec3 normal;
attribute vec2 uv;

uniform mat4 ProjMatrix;
uniform mat4 TransMatrix;
uniform mat4 NormalMatrix;

uniform vec4 light;

varying vec4 color;

void
main(void)
{
  vec3 n = normalize(vec3(NormalMatrix * vec4(normal,1.0)));
  vec3 l = normalize(light.xyz);
  float diffuse = max(dot(n,l),0.0);
  color = vec4(diffuse,diffuse,diffuse,1.0);

  //color = vec4(n,1);

  //color = vec4(normal,1);

  //color = vec4(1,0,0,1);

  vec4 v = TransMatrix * vec4(vertex.x,vertex.y,vertex.z,1);
  gl_Position = ProjMatrix * v;
}
