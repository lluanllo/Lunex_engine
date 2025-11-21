#include "stpch.h"
#include "Light.h"

namespace Lunex {

	Light::Light()
		: m_Type(LightType::Point),
		  m_Color(1.0f, 1.0f, 1.0f),
		  m_Intensity(1.0f),
		  m_Range(10.0f),
		  m_Attenuation(1.0f, 0.09f, 0.032f),
		  m_InnerConeAngle(12.5f),
		  m_OuterConeAngle(17.5f),
		  m_CastShadows(true)
	{
	}

	Light::Light(LightType type)
		: m_Type(type),
		  m_Color(1.0f, 1.0f, 1.0f),
		  m_Intensity(1.0f),
		  m_Range(10.0f),
		  m_Attenuation(1.0f, 0.09f, 0.032f),
		  m_InnerConeAngle(12.5f),
		  m_OuterConeAngle(17.5f),
		  m_CastShadows(true)
	{
	}

}
