// Vertex shader
#version 150
#extension GL_ARB_explicit_attrib_location : require

layout(location = 0) in vec4 a_position;

out vec3 v_texture_coordinate;

uniform mat4 u_view_transpose; // Transposed view matrix
uniform float u_fovy;
uniform float u_aspect;

void main() 
{
	gl_Position = a_position;

	vec3 vs_ray_dir = vec3(
		tan(u_fovy/2.0)*u_aspect*a_position.x, 
		tan(u_fovy/2.0)*a_position.y,
		-1.0);
	v_texture_coordinate = mat3(u_view_transpose) * vs_ray_dir;
}
