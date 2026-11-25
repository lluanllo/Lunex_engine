#version 450 core

/**
 * Deferred Rendering - Geometry Pass
 * 
 * Writes material properties into G-Buffer MRT:
 *   RT0 (RGBA16F): Albedo.rgb   + Metallic
 *   RT1 (RGBA16F): Normal.xyz   + Roughness  (world-space normals)
 *   RT2 (RGBA16F): Emission.rgb + AO
 *   RT3 (RGBA16F): Position.xyz + Specular   (world-space position)
 *   RT4 (R32I):    EntityID
 *
 * This shader does NO lighting - only material evaluation and G-Buffer write.
 */

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;
layout(location = 5) in int a_EntityID;
layout(location = 6) in ivec4 a_BoneIDs;
layout(location = 7) in vec4  a_BoneWeights;

// Bone palette of the mesh being drawn (only read when u_Skinned != 0)
layout(std430, binding = 4) readonly buffer BoneMatrices {
	mat4 u_Bones[];
};

layout(std140, binding = 0) uniform Camera {
	mat4 u_ViewProjection;
	mat4 u_View;
	mat4 u_Projection;
	vec3 u_ViewPos;
	float _cameraPad;
};

layout(std140, binding = 1) uniform Transform {
	mat4 u_Transform;
	mat4 u_NormalMatrix;   // transpose(inverse(u_Transform)), computed on CPU
	int  u_EntityID;
	int  u_Skinned;
	int  u_DebugBone;      // >= 0: visualize this bone's weights
};

mat4 GetSkinMatrix() {
	if (u_Skinned == 0) return mat4(1.0);
	return a_BoneWeights.x * u_Bones[a_BoneIDs.x] +
	       a_BoneWeights.y * u_Bones[a_BoneIDs.y] +
	       a_BoneWeights.z * u_Bones[a_BoneIDs.z] +
	       a_BoneWeights.w * u_Bones[a_BoneIDs.w];
}

struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	mat3 TBN;
};

layout (location = 0) out VertexOutput Output;
layout (location = 7) out flat int v_EntityID;
layout (location = 9) out float v_DebugWeight;   // < 0 = debug view off

void main() {
	mat4 skin = GetSkinMatrix();
	vec4 worldPos = u_Transform * (skin * vec4(a_Position, 1.0));
	Output.FragPos = worldPos.xyz;
	
	mat3 normalMatrix = mat3(u_NormalMatrix);
	vec3 N = normalize(normalMatrix * (mat3(skin) * a_Normal));
	Output.Normal = N;
	Output.TexCoords = a_TexCoords;
	
	vec3 T = normalize(normalMatrix * (mat3(skin) * a_Tangent));
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T) * sign(dot(cross(a_Normal, a_Tangent), a_Bitangent));
	Output.TBN = mat3(T, B, N);
	
	v_EntityID = u_EntityID;

	v_DebugWeight = -1.0;
	if (u_DebugBone >= 0) {
		v_DebugWeight = 0.0;
		for (int k = 0; k < 4; k++)
			if (a_BoneIDs[k] == u_DebugBone) v_DebugWeight += a_BoneWeights[k];
	}
	
	gl_Position = u_ViewProjection * worldPos;
}

#elif defined(FRAGMENT)

// G-Buffer MRT outputs
layout(location = 0) out vec4 gAlbedoMetallic;    // RT0
layout(location = 1) out vec4 gNormalRoughness;    // RT1
layout(location = 2) out vec4 gEmissionAO;         // RT2
layout(location = 3) out vec4 gPositionSpecular;   // RT3
layout(location = 4) out int  gEntityID;           // RT4
layout(location = 5) out vec4 gSubsurface;         // RT5: color.rgb + amount

struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	mat3 TBN;
};

layout (location = 0) in VertexOutput Input;
layout (location = 7) in flat int v_EntityID;
layout (location = 9) in float v_DebugWeight;

vec3 WeightHeatmap(float w) {
	w = clamp(w, 0.0, 1.0);
	return w < 0.5 ? mix(vec3(0.05, 0.1, 0.9), vec3(0.1, 0.9, 0.2), w * 2.0)
	               : mix(vec3(0.1, 0.9, 0.2), vec3(1.0, 0.1, 0.05), (w - 0.5) * 2.0);
}

// ============ MATERIAL PROPERTIES ============
layout(std140, binding = 2) uniform Material {
	vec4 u_Color;
	float u_Metallic;
	float u_Roughness;
	float u_Specular;
	float u_EmissionIntensity;
	vec3 u_EmissionColor;
	float u_NormalIntensity;
	vec3 u_ViewPos;
	
	int u_UseAlbedoMap;
	int u_UseNormalMap;
	int u_UseMetallicMap;
	int u_UseRoughnessMap;
	int u_UseSpecularMap;
	int u_UseEmissionMap;
	int u_UseAOMap;
	float _padding2;
	
	float u_MetallicMultiplier;
	float u_RoughnessMultiplier;
	float u_SpecularMultiplier;
	float u_AOMultiplier;

	int u_DetailNormalCount;
	int u_UseLayeredTexture;
	int u_LayeredMetallicChannel;
	int u_LayeredRoughnessChannel;

	int u_LayeredAOChannel;
	int u_LayeredUseMetallic;
	int u_LayeredUseRoughness;
	int u_LayeredUseAO;

	float _detailPad0;

	vec4 u_DetailNormalIntensities;
	vec4 u_DetailNormalTilingX;
	vec4 u_DetailNormalTilingY;

	vec4 u_SubsurfaceColorAmount;   // rgb = scatter color, a = amount
	vec4 u_UVTilingOffset;          // xy = tiling, zw = offset
};

// Material UV transform (tiling 0 = legacy upload without the field -> identity)
vec2 MaterialUV() {
	vec2 tiling = (u_UVTilingOffset.x == 0.0 && u_UVTilingOffset.y == 0.0) ? vec2(1.0) : u_UVTilingOffset.xy;
	return Input.TexCoords * tiling + u_UVTilingOffset.zw;
}


// ============ TEXTURE SAMPLERS ============
layout (binding = 0) uniform sampler2D u_AlbedoMap;
layout (binding = 1) uniform sampler2D u_NormalMap;
layout (binding = 2) uniform sampler2D u_MetallicMap;
layout (binding = 3) uniform sampler2D u_RoughnessMap;
layout (binding = 4) uniform sampler2D u_SpecularMap;
layout (binding = 5) uniform sampler2D u_EmissionMap;
layout (binding = 6) uniform sampler2D u_AOMap;
layout (binding = 7) uniform sampler2D u_LayeredMap;

// ============ DETAIL NORMAL MAP SAMPLERS ============
layout (binding = 12) uniform sampler2D u_DetailNormal0;
layout (binding = 13) uniform sampler2D u_DetailNormal1;
layout (binding = 14) uniform sampler2D u_DetailNormal2;
layout (binding = 15) uniform sampler2D u_DetailNormal3;

// ============ CONSTANTS ============
const float MIN_ROUGHNESS = 0.045;
const float EPSILON = 0.00001;

// ============ HELPERS ============

float SampleLayeredChannel(vec4 layeredSample, int channel) {
	if (channel == 0) return layeredSample.r;
	if (channel == 1) return layeredSample.g;
	if (channel == 2) return layeredSample.b;
	return layeredSample.a;
}

vec3 BlendNormals_UDN(vec3 base, vec3 detail) {
	return normalize(vec3(base.xy + detail.xy, base.z));
}

vec3 SampleDetailNormal(sampler2D detailMap, vec2 uv, float tilingX, float tilingY) {
	vec2 detailUV = uv * vec2(tilingX, tilingY);
	vec3 n = texture(detailMap, detailUV).rgb;
	return n * 2.0 - 1.0;
}

void main() {
	// ========== ALBEDO ==========
	vec3 albedo;
	float alpha = u_Color.a;
	if (u_UseAlbedoMap != 0) {
		vec4 texColor = texture(u_AlbedoMap, MaterialUV());
		albedo = texColor.rgb * u_Color.rgb;
		alpha *= texColor.a;
	} else {
		albedo = u_Color.rgb;
	}
	
	// Alpha test
	if (alpha < 0.01) discard;
	
	// ========== NORMAL MAPPING ==========
	vec3 N;
	if (u_UseNormalMap != 0) {
		vec3 normalMap = texture(u_NormalMap, MaterialUV()).rgb;
		normalMap = normalMap * 2.0 - 1.0;
		normalMap.xy *= u_NormalIntensity;
		normalMap = normalize(normalMap);
		
		// Blend detail normals
		if (u_DetailNormalCount > 0) {
			vec3 d = SampleDetailNormal(u_DetailNormal0, MaterialUV(), u_DetailNormalTilingX.x, u_DetailNormalTilingY.x);
			d.xy *= u_DetailNormalIntensities.x;
			normalMap = BlendNormals_UDN(normalMap, d);
		}
		if (u_DetailNormalCount > 1) {
			vec3 d = SampleDetailNormal(u_DetailNormal1, MaterialUV(), u_DetailNormalTilingX.y, u_DetailNormalTilingY.y);
			d.xy *= u_DetailNormalIntensities.y;
			normalMap = BlendNormals_UDN(normalMap, d);
		}
		if (u_DetailNormalCount > 2) {
			vec3 d = SampleDetailNormal(u_DetailNormal2, MaterialUV(), u_DetailNormalTilingX.z, u_DetailNormalTilingY.z);
			d.xy *= u_DetailNormalIntensities.z;
			normalMap = BlendNormals_UDN(normalMap, d);
		}
		if (u_DetailNormalCount > 3) {
			vec3 d = SampleDetailNormal(u_DetailNormal3, MaterialUV(), u_DetailNormalTilingX.w, u_DetailNormalTilingY.w);
			d.xy *= u_DetailNormalIntensities.w;
			normalMap = BlendNormals_UDN(normalMap, d);
		}
		
		N = normalize(Input.TBN * normalMap);
	} else {
		N = normalize(Input.Normal);
		
		// Detail normals without base normal map
		if (u_DetailNormalCount > 0) {
			vec3 detailAccum = vec3(0.0, 0.0, 1.0);
			if (u_DetailNormalCount > 0) {
				vec3 d = SampleDetailNormal(u_DetailNormal0, MaterialUV(), u_DetailNormalTilingX.x, u_DetailNormalTilingY.x);
				d.xy *= u_DetailNormalIntensities.x;
				detailAccum = BlendNormals_UDN(detailAccum, d);
			}
			if (u_DetailNormalCount > 1) {
				vec3 d = SampleDetailNormal(u_DetailNormal1, MaterialUV(), u_DetailNormalTilingX.y, u_DetailNormalTilingY.y);
				d.xy *= u_DetailNormalIntensities.y;
				detailAccum = BlendNormals_UDN(detailAccum, d);
			}
			if (u_DetailNormalCount > 2) {
				vec3 d = SampleDetailNormal(u_DetailNormal2, MaterialUV(), u_DetailNormalTilingX.z, u_DetailNormalTilingY.z);
				d.xy *= u_DetailNormalIntensities.z;
				detailAccum = BlendNormals_UDN(detailAccum, d);
			}
			if (u_DetailNormalCount > 3) {
				vec3 d = SampleDetailNormal(u_DetailNormal3, MaterialUV(), u_DetailNormalTilingX.w, u_DetailNormalTilingY.w);
				d.xy *= u_DetailNormalIntensities.w;
				detailAccum = BlendNormals_UDN(detailAccum, d);
			}
			N = normalize(Input.TBN * detailAccum);
		}
	}
	
	// ========== LAYERED TEXTURE SAMPLING ==========
	vec4 layeredSample = vec4(0.0);
	if (u_UseLayeredTexture != 0) {
		layeredSample = texture(u_LayeredMap, MaterialUV());
	}
	
	// ========== METALLIC ==========
	float metallic = u_Metallic;
	if (u_UseLayeredTexture != 0 && u_LayeredUseMetallic != 0) {
		metallic = clamp(SampleLayeredChannel(layeredSample, u_LayeredMetallicChannel) * u_MetallicMultiplier, 0.0, 1.0);
	} else if (u_UseMetallicMap != 0) {
		metallic = clamp(texture(u_MetallicMap, MaterialUV()).r * u_MetallicMultiplier, 0.0, 1.0);
	}
	
	// ========== ROUGHNESS ==========
	float roughness = u_Roughness;
	if (u_UseLayeredTexture != 0 && u_LayeredUseRoughness != 0) {
		roughness = clamp(SampleLayeredChannel(layeredSample, u_LayeredRoughnessChannel) * u_RoughnessMultiplier, 0.0, 1.0);
	} else if (u_UseRoughnessMap != 0) {
		roughness = clamp(texture(u_RoughnessMap, MaterialUV()).r * u_RoughnessMultiplier, 0.0, 1.0);
	}
	roughness = max(roughness, MIN_ROUGHNESS);
	
	// ========== SPECULAR ==========
	float specular = u_Specular;
	if (u_UseSpecularMap != 0) {
		specular = clamp(texture(u_SpecularMap, MaterialUV()).r * u_SpecularMultiplier, 0.0, 1.0);
	}
	
	// ========== AMBIENT OCCLUSION ==========
	float ao = 1.0;
	if (u_UseLayeredTexture != 0 && u_LayeredUseAO != 0) {
		ao = clamp(SampleLayeredChannel(layeredSample, u_LayeredAOChannel) * u_AOMultiplier, 0.0, 1.0);
	} else if (u_UseAOMap != 0) {
		ao = clamp(texture(u_AOMap, MaterialUV()).r * u_AOMultiplier, 0.0, 1.0);
	}
	
	// ========== EMISSION ==========
	vec3 emission = vec3(0.0);
	if (u_EmissionIntensity > EPSILON) {
		emission = u_EmissionColor * u_EmissionIntensity;
		if (u_UseEmissionMap != 0) {
			vec3 emissionTex = texture(u_EmissionMap, MaterialUV()).rgb;
			emission = emissionTex * u_EmissionColor * u_EmissionIntensity;
		}
	}
	
	// ========== WRITE G-BUFFER ==========
	if (v_DebugWeight >= 0.0) {
		// Bone weight view: unlit heatmap through the emission channel
		albedo = vec3(0.0);
		metallic = 0.0;
		emission = WeightHeatmap(v_DebugWeight) * 1.5;
	}
	gAlbedoMetallic   = vec4(albedo, metallic);
	gNormalRoughness  = vec4(N * 0.5 + 0.5, roughness);   // Encode normals to [0,1]
	gEmissionAO       = vec4(emission, ao);
	gPositionSpecular = vec4(Input.FragPos, specular);
	gEntityID         = v_EntityID;
	gSubsurface       = (v_DebugWeight >= 0.0) ? vec4(0.0) : u_SubsurfaceColorAmount;
}

#endif
