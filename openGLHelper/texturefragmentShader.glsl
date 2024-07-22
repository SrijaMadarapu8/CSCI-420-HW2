#version 150

in vec3 tc;
out vec4 c;
uniform samplerCube textureImage;

void main()
{
  c = texture(textureImage,tc);
}