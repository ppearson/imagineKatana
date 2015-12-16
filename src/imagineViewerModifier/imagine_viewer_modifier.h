#ifndef IMAGINEVIEWERMODIFIER_H
#define IMAGINEVIEWERMODIFIER_H

#ifdef KAT_V_2
#include <FnViewerModifier/plugin/FnViewerModifier.h>
#include <FnViewerModifier/plugin/FnViewerModifierInput.h>
#else
#include <FnViewerModifier/FnViewerModifier.h>
#include <FnViewerModifier/FnViewerModifierInput.h>
#endif

#include <FnAttribute/FnGroupBuilder.h>

class ImagineViewerModifier : public Foundry::Katana::ViewerModifier
{
public:
	ImagineViewerModifier(Foundry::Katana::GroupAttribute args);

	static Foundry::Katana::ViewerModifier* create(Foundry::Katana::GroupAttribute args)
	{
		return (Foundry::Katana::ViewerModifier*)new ImagineViewerModifier(args);
	}

	static Foundry::Katana::GroupAttribute getArgumentTemplate()
	{
		Foundry::Katana::GroupBuilder gb;
		return gb.build();
	}

	static const char* getLocationType()
	{
		return "light";
	}

	static void flush()
	{

	}

	virtual void deepSetup(Foundry::Katana::ViewerModifierInput& input);

	virtual void setup(Foundry::Katana::ViewerModifierInput& input);

	virtual void draw(Foundry::Katana::ViewerModifierInput& input);

	virtual void cleanup(Foundry::Katana::ViewerModifierInput& input)
	{

	}

	virtual void deepCleanup(Foundry::Katana::ViewerModifierInput& input)
	{

	}

	Foundry::Katana::DoubleAttribute getLocalSpaceBoundingBox(Foundry::Katana::ViewerModifierInput& input)
	{
		double bounds[6] = {-1.0, 1.0, -1.0, 1.0, -1.0, 1.0};
		return FnKat::DoubleAttribute(bounds, 6, 1);
	}

protected:
	enum LightType
	{
		eLightPoint,
		eLightArea,
		eLightSpot,
		eLightDistant,
		eLightEnvironment
	};

	enum AreaLightType
	{
		eAreaLightRect,
		eAreaLightDisc,
		eAreaLightCylinder,
		eAreaLightSphere
	};

	LightType		m_lightType;
	AreaLightType	m_areaLightType;

	float			m_width;
	float			m_depth;

	float			m_coneAngle;
};

#endif // IMAGINEVIEWERMODIFIER_H
