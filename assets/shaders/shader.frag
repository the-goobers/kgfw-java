//precision mediump float;
varying vec3 v_pos;
varying vec3 v_color;
varying vec3 v_normal;
varying vec2 v_uv;

uniform float unif_time;
uniform vec3 unif_view_pos;
uniform float unif_textured_color;
uniform float unif_textured_normal;
uniform sampler2D unif_texture_color;
uniform sampler2D unif_texture_normal;

//varying vec4 out_color;

#define FOG_MIN -20.0
#define FOG_MAX 500.0

void main() {
	vec4 tex = texture2D(unif_texture_color, v_uv);
	vec4 col = mix(vec4( v_color /*(v_normal + 1.0) / 2.0*/, 1.0), tex, unif_textured_color);
	if (col.a == 0.0) {
		discard;
	}
	/*//col = mix(col, vec4((v_uv.xyx + 1.0) / 2.0, 1.0), (1.0 - unif_textured_color) * 0.5);

	vec3 light_pos = vec3(0.0, 1000.0, 0.0);
	float light_power = 100000.0;
	vec3 dir = light_pos - v_pos;
	float dist = length(dir);
	dir = normalize(dir);
	float cam_dist = length(unif_view_pos - v_pos);

	vec3 light_color = vec3(1.0, 1.0, 1.0);
	vec3 ambient_color = vec3(1.0, 1.0, 1.0);
	vec3 fog_color = v_normal;

	float ambience = 0.5;
	float diffusion = 0.8;
	float shiny = 24.0;

	float lambertian = max(dot(dir, v_normal), 0.0) * diffusion;
	
	float specular = 0.0;
	if (lambertian > 0.0) {
		vec3 view_dir = normalize(-v_pos);
		vec3 half_dir = normalize(dir + view_dir);
		float spec = max(dot(half_dir, v_normal), 0.0);
		specular = pow(spec, shiny);
	}

	vec3 color = mix(col.xyz * ((ambient_color * ambience) + (light_color * lambertian * light_power / (dist * dist)) + (light_color * specular * light_power / (dist * dist))), fog_color, 1.0 - (FOG_MAX - cam_dist) / (FOG_MAX - FOG_MIN));
	*/
	gl_FragColor = col;/*vec4((v_normal + 1.0) / 2.0, 1.0);*//*vec4(v_pos, 1.0);*//*vec4(v_uv, 0.0, 1.0);*/
}
