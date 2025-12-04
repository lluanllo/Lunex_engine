#version 450 core

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	vec3 u_CameraPosition;
};

layout(location = 0) out vec3 v_NearPoint;
layout(location = 1) out vec3 v_FarPoint;

// Desprojectar el punto del NDC a espacio de mundo
vec3 UnprojectPoint(float x, float y, float z, mat4 invViewProj) {
	vec4 unprojectedPoint = invViewProj * vec4(x, y, z, 1.0);
	return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
	mat4 invViewProj = inverse(u_ViewProjection);

	// Convertir el quad fullscreen de clip space a world space
	// Near plane (z = 0 en NDC, mapea a near clip)
	v_NearPoint = UnprojectPoint(a_Position.x, a_Position.y, 0.0, invViewProj);
	// Far plane (z = 1 en NDC, mapea a far clip)
	v_FarPoint = UnprojectPoint(a_Position.x, a_Position.y, 1.0, invViewProj);

	gl_Position = vec4(a_Position, 1.0);
}

#endif

#ifdef FRAGMENT

layout(location = 0) out vec4 FragColor;
layout(location = 1) out int EntityID;

layout(location = 0) in vec3 v_NearPoint;
layout(location = 1) in vec3 v_FarPoint;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	vec3 u_CameraPosition;
};

layout(std140, binding = 1) uniform GridSettings
{
	vec3 u_XAxisColor;
	float u_GridScale;
	vec3 u_ZAxisColor;
	float u_MinorLineThickness;
	vec3 u_GridColor;
	float u_MajorLineThickness;
	float u_FadeDistance;
};

// Calcular la profundidad correcta para el fragment
float ComputeDepth(vec3 pos) {
	vec4 clipSpacePos = u_ViewProjection * vec4(pos, 1.0);
	// Convertir de clip space a depth NDC [0, 1]
	return (clipSpacePos.z / clipSpacePos.w) * 0.5 + 0.5;
}

// Función de distancia para el grid con LOD adaptativo
vec4 Grid(vec3 fragPos3D, float scale, float lineThickness) {
	vec2 coord = fragPos3D.xz * scale;

	// Derivadas para anti-aliasing
	vec2 derivative = fwidth(coord);

	// Grid con LOD: líneas más gruesas cuando estamos lejos
	vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
	float gridLine = min(grid.x, grid.y);

	// Calcular nivel de LOD basado en distancia
	float distanceToCamera = length(u_CameraPosition - fragPos3D);
	float lod = log2(distanceToCamera * 0.1 + 1.0);

	// Ajustar grosor basado en LOD y zoom
	float adaptiveThickness = lineThickness * (1.0 + lod * 0.3);

	// Suavizar las líneas con smoothstep
	float minLine = min(gridLine, 1.0);
	float lineAlpha = 1.0 - smoothstep(adaptiveThickness, adaptiveThickness + derivative.x, minLine);

	vec4 color = vec4(u_GridColor, lineAlpha);
	return color;
}

// Grid para líneas principales (cada 10 unidades)
vec4 MajorGrid(vec3 fragPos3D, float scale, float lineThickness) {
	vec2 coord = fragPos3D.xz * scale / 10.0; // División por 10 para líneas mayores

	vec2 derivative = fwidth(coord);
	vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
	float gridLine = min(grid.x, grid.y);

	float distanceToCamera = length(u_CameraPosition - fragPos3D);
	float lod = log2(distanceToCamera * 0.1 + 1.0);

	float adaptiveThickness = lineThickness * (1.0 + lod * 0.3);

	float minLine = min(gridLine, 1.0);
	float lineAlpha = 1.0 - smoothstep(adaptiveThickness, adaptiveThickness + derivative.x, minLine);

	// Las líneas mayores son más oscuras (más visibles)
	vec3 majorColor = u_GridColor * 0.6;
	vec4 color = vec4(majorColor, lineAlpha);
	return color;
}

void main() {
	// ? CRITICAL: Write -1 to entity ID to prevent grid selection
	EntityID = -1;
	
	// Intersección del rayo con el plano Y=0 (plano horizontal)
	float t = -v_NearPoint.y / (v_FarPoint.y - v_NearPoint.y);

	// Si no intersecta el plano, descartar
	if (t < 0.0) {
		discard;
	}

	vec3 fragPos3D = v_NearPoint + t * (v_FarPoint - v_NearPoint);

	// Calcular profundidad para el depth buffer
	float depth = ComputeDepth(fragPos3D);
	gl_FragDepth = depth;

	// Calcular distancia a la cámara para fade
	float distanceToCamera = length(u_CameraPosition - fragPos3D);
	float fadeFactor = 1.0 - smoothstep(u_FadeDistance * 0.5, u_FadeDistance, distanceToCamera);

	// Si está muy lejos, descartar
	if (fadeFactor <= 0.01) {
		discard;
	}

	// Grid menor (cada unidad)
	vec4 minorGrid = Grid(fragPos3D, u_GridScale, u_MinorLineThickness);

	// Grid mayor (cada 10 unidades)
	vec4 majorGrid = MajorGrid(fragPos3D, u_GridScale, u_MajorLineThickness);

	// Combinar grids
	vec4 gridColor = mix(minorGrid, majorGrid, majorGrid.a);

	// Ejes X y Z con colores suaves
	float axisThickness = u_MajorLineThickness * 2.0;

	// Eje X (rojo suave)
	float xAxisDist = abs(fragPos3D.z);
	float xAxisDerivative = fwidth(fragPos3D.z * u_GridScale);
	float xAxisLine = 1.0 - smoothstep(axisThickness, axisThickness + xAxisDerivative, xAxisDist * u_GridScale);
	vec4 xAxisColor = vec4(u_XAxisColor, xAxisLine);

	// Eje Z (azul suave)
	float zAxisDist = abs(fragPos3D.x);
	float zAxisDerivative = fwidth(fragPos3D.x * u_GridScale);
	float zAxisLine = 1.0 - smoothstep(axisThickness, axisThickness + zAxisDerivative, zAxisDist * u_GridScale);
	vec4 zAxisColor = vec4(u_ZAxisColor, zAxisLine);

	// Combinar todo: grid + ejes
	vec4 finalColor = gridColor;

	// Mezclar ejes sobre el grid
	finalColor = mix(finalColor, xAxisColor, xAxisColor.a);
	finalColor = mix(finalColor, zAxisColor, zAxisColor.a);

	// Aplicar fade por distancia
	finalColor.a *= fadeFactor;

	// Si el alpha es muy bajo, descartar para mejor performance
	if (finalColor.a <= 0.01) {
		discard;
	}

	FragColor = finalColor;
}

#endif
