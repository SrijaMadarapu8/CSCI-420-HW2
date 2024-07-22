#version 150

in vec3 position;

out vec3 tc;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;


void main()
{
  // compute the transformed and projected vertex position (into gl_Position) 
  // compute the vertex color (into col)
  vec4 pos  = projectionMatrix * modelViewMatrix *vec4(position, 1.0f);
    gl_Position = pos.xyww;
  tc = position;
}