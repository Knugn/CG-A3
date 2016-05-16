// Fragment shader
#version 150

in vec3 v_normal;
in vec3 v_light;
in vec3 v_viewer;

out vec4 frag_color;

uniform vec3 u_ambient_light;
uniform vec3 u_light_color;

uniform vec3 u_diffuse_color, u_specular_color;
uniform float u_specular_power;

uniform float u_ambient_weight;
uniform float u_diffuse_weight;
uniform float u_specular_weight;

#define CM_NORMAL_AS_RGB	0
#define CM_BLINN_PHONG		1
#define CM_REFLECTION		2
uniform int u_color_mode;
uniform int u_use_gamma_correction;
uniform int u_use_color_inversion;

uniform samplerCube u_cubemap;
//uniform float u_cubemap_lod;

vec3 blinn_phong(vec3 N, vec3 L, vec3 H) 
{
	vec3 ambient_intensity = u_ambient_light;
	vec3 ambient_term = u_diffuse_color * ambient_intensity;

	vec3 diffuse_intensity = u_light_color * max(0.0, dot(N, L));
	vec3 diffuse_term = u_diffuse_color * diffuse_intensity;

	// Normalization factor from "Real-Time Rendering" book, 2nd ed.
	float specular_norm = (8.0 + u_specular_power) / 8.0;
	vec3 specular_intensity = u_light_color * max(0.0, pow(dot(N, H), u_specular_power)) * specular_norm;
	vec3 specular_term = u_specular_color * specular_intensity;

	mat3 color_matrix = mat3(ambient_term, diffuse_term, specular_term);
	vec3 weights = vec3(u_ambient_weight, u_diffuse_weight, u_specular_weight);
	vec3 combined = color_matrix * weights;
	//vec3 combined = ambient_term + diffuse_term + specular_term;
	return combined;
}

vec3 gamma_correction(vec3 linear_color)
{
	return pow(linear_color, vec3(1.0/2.2));
}

void main()
{
    vec3 N = normalize(v_normal);
	vec3 L = normalize(v_light);
	vec3 V = normalize(v_viewer);
	vec3 H = normalize(L+V);
	vec3 R = reflect(-V,N);

	vec3 color;
	switch(u_color_mode) {
		case CM_NORMAL_AS_RGB:
			color = 0.5 * N + 0.5;
			break;
		case CM_BLINN_PHONG:
			color = blinn_phong(N, L, H);
			break;
		case CM_REFLECTION:
			color = texture(u_cubemap, R).rgb;
			break;
		default:
			break;
	}

	if (u_use_gamma_correction != 0)
		color = gamma_correction(color);

	if (u_use_color_inversion != 0)
		color = vec3(1.0) - color;
	
	frag_color = vec4(color, 1.0);
}
