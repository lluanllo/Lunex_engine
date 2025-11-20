#include "stpch.h"
#include "Material.h"

namespace Lunex {

	Material::Material()
		: m_Color(1.0f, 1.0f, 1.0f, 1.0f),
		  m_Metallic(0.0f),
		  m_Roughness(0.5f),
		  m_Specular(0.5f),
		  m_EmissionColor(0.0f, 0.0f, 0.0f),
		  m_EmissionIntensity(0.0f)
	{
	}

	Material::Material(const glm::vec4& color)
		: m_Color(color),
		  m_Metallic(0.0f),
		  m_Roughness(0.5f),
		  m_Specular(0.5f),
		  m_EmissionColor(0.0f, 0.0f, 0.0f),
		  m_EmissionIntensity(0.0f)
	{
	}

}
