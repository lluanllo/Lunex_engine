#version 450 core

#ifdef VERTEX
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoords;

layout(location = 0) out vec2 v_TexCoords;

void main() {
	v_TexCoords = a_TexCoords;
	gl_Position = vec4(a_Position, 1.0);
}

#elif defined(FRAGMENT)
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 v_TexCoords;

layout(binding = 0) uniform sampler2D u_Texture;

void main() {
	vec4 texColor = texture(u_Texture, v_TexCoords);
	
	// Solo dibujar si hay alpha > 0 (si es outline)
	// Si es transparente (alpha == 0), descartar y mantener lo que hay debajo
	if (texColor.a < 0.01)
		discard;
	
	FragColor = texColor;
}
#endif
