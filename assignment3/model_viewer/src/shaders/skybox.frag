// Fragment shader
#version 150

in vec3 v_texture_coordinate;

out vec4 frag_color;

uniform samplerCube u_cubemap;

void main()
{
	vec3 color = texture(u_cubemap, v_texture_coordinate).rgb;
	//color = vec3(0.0,1.0,0.0);
	//color = v_texture_coordinate*0.5 + 0.5;
	frag_color = vec4(color, 1.0);
}
