#version 460 core

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

// Textura con el ID de la entidad seleccionada (entero)
layout(binding = 0) uniform isampler2D u_EntityIDTexture;

// Uniform buffer en binding=3 para no colisionar con Camera (0), Transform (1), Material (2)
layout(std140, binding = 3) uniform OutlineSettings {
	vec4 u_OutlineColor;
	int u_SelectedEntityID;
	float u_OutlineThickness;
	vec2 u_ViewportSize;
};

void main() {
	// Leer el entity ID del pixel actual (entero)
	int currentID = texture(u_EntityIDTexture, v_TexCoords).r;
	
	// Si no es la entidad seleccionada, descartamos
	if (currentID != u_SelectedEntityID) {
		discard;
	}
	
	// Calcular el tamaño de un pixel en coordenadas de textura
	vec2 texelSize = 1.0 / u_ViewportSize;
	float thickness = u_OutlineThickness;
	
	// Kernel de detección de bordes (búsqueda vecinal)
	bool isEdge = false;
	for (float x = -thickness; x <= thickness; x += 1.0) {
		for (float y = -thickness; y <= thickness; y += 1.0) {
			if (x == 0.0 && y == 0.0) continue;
			vec2 offset = vec2(x, y) * texelSize;
			int neighborID = texture(u_EntityIDTexture, v_TexCoords + offset).r;
			if (neighborID != u_SelectedEntityID) {
				isEdge = true;
				break;
			}
		}
		if (isEdge) break;
	}
	
	// Si es borde, dibujar outline, si no descartar
	if (isEdge) {
		FragColor = u_OutlineColor;
	} else {
		discard;
	}
}
#endif