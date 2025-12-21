#include "stpch.h"
#include "Light.h"

namespace Lunex {

	Light::Light()
		: m_Properties()
	{
	}

	Light::Light(LightType type)
		: m_Properties()
	{
		m_Properties.Type = type;
	}

} // namespace Lunex
