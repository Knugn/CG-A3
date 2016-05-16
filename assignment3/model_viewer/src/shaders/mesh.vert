// Vertex shader
#version 150
#extension GL_ARB_explicit_attrib_location : require

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec3 a_normal;

out vec3 v_normal;
out vec3 v_light;
out vec3 v_viewer;

uniform mat4 u_v; // View matrix
uniform mat4 u_mv; // Model-View matrix
uniform mat4 u_mvp; // Model-View-Projection matrix

uniform vec3 u_light_position;

void main()
{
	vec3 vs_vertex_position = mat3(u_mv) * a_position.xyz;
	vec3 vs_light_position = mat3(u_v) * u_light_position;
	v_normal = mat3(u_mv) * a_normal;
	v_light = vs_light_position - vs_vertex_position;
	v_viewer = -vs_vertex_position;

	gl_Position = u_mvp * vec4(a_position.xyz, 1.0);
}
