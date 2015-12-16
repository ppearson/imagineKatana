#include "imagine_viewer_modifier.h"

#ifdef __linux__
#include <GL/gl.h>
#include <GL/glu.h>
#else
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#endif

#include <stdio.h>
#include <cmath>

static const float kPI = 3.14159f;

ImagineViewerModifier::ImagineViewerModifier(Foundry::Katana::GroupAttribute args) : FnKat::ViewerModifier(args),
	m_lightType(eLightPoint), m_areaLightType(eAreaLightRect), m_width(1.0f), m_depth(1.0f), m_coneAngle(35.0f)
{

}

void ImagineViewerModifier::deepSetup(Foundry::Katana::ViewerModifierInput& input)
{
	input.overrideHostGeometry();
}

void ImagineViewerModifier::setup(Foundry::Katana::ViewerModifierInput& input)
{
	// work out what light type we are...

	Foundry::Katana::GroupAttribute lightMatAttribute = input.getAttribute("material");

	if (!lightMatAttribute.isValid())
		return;

	// get overall type
	Foundry::Katana::StringAttribute lightTypeAttribute = lightMatAttribute.getChildByName("imagineLightShader");

	if (!lightTypeAttribute.isValid())
	{
		fprintf(stderr, "ImagineViewerModifier: Can't find shader type\n");
		return;
	}

	std::string lightTypeStr = lightTypeAttribute.getValue("point", false);

	if (lightTypeStr == "Area")
	{
		m_lightType = eLightArea;
	}
	else if (lightTypeStr == "Point")
	{
		m_lightType = eLightPoint;
	}
	else if (lightTypeStr == "Spot")
	{
		m_lightType = eLightSpot;
	}

	// now get the params for the actual shader
	Foundry::Katana::GroupAttribute lightParamsAttribute = lightMatAttribute.getChildByName("imagineLightParams");

	if (!lightParamsAttribute.isValid())
	{
		return;
	}

	if (m_lightType == eLightArea)
	{
		Foundry::Katana::StringAttribute areaShapeTypeAttribute = lightParamsAttribute.getChildByName("shape_type");
		if (areaShapeTypeAttribute.isValid())
		{
			std::string areaShapeTypeStr = areaShapeTypeAttribute.getValue("quad", false);
			if (areaShapeTypeStr == "quad")
			{
				m_areaLightType = eAreaLightRect;
			}
			else if (areaShapeTypeStr == "disc")
			{
				m_areaLightType = eAreaLightDisc;
			}
			else if (areaShapeTypeStr == "cylinder")
			{
				m_areaLightType = eAreaLightCylinder;
			}
			else if (areaShapeTypeStr == "sphere")
			{
				m_areaLightType = eAreaLightSphere;
			}
		}

		Foundry::Katana::FloatAttribute widthAttribute = lightParamsAttribute.getChildByName("width");
		if (widthAttribute.isValid())
		{
			m_width = widthAttribute.getValue(1.0f, false);
		}

		Foundry::Katana::FloatAttribute depthAttribute = lightParamsAttribute.getChildByName("depth");
		if (depthAttribute.isValid())
		{
			m_depth = depthAttribute.getValue(1.0f, false);
		}
	}
	else if (m_lightType == eLightSpot)
	{
		Foundry::Katana::FloatAttribute coneAngleAttribute = lightParamsAttribute.getChildByName("cone_angle");
		if (coneAngleAttribute.isValid())
		{
			m_coneAngle = coneAngleAttribute.getValue(35.0f, false);
		}
	}
}

void ImagineViewerModifier::draw(Foundry::Katana::ViewerModifierInput& input)
{
	glPushAttrib(GL_POLYGON_BIT | GL_LIGHTING_BIT | GL_LINE_BIT);

	glDisable(GL_LIGHTING);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	if (input.isSelected())
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	else
		glColor4f(1.0f, 1.0f, 0.0f, 1.0f);

	glPushMatrix();

	if (m_lightType == eLightArea)
	{
		float halfWidth = m_width * 0.5f;
		float halfDepth = m_depth * 0.5f;

		// Imagine's lights are non-standard, defaultly pointing down Y

		bool drawDirection = false;

		if (m_areaLightType == eAreaLightRect)
		{
			glBegin(GL_POLYGON);
				glVertex3f(-halfWidth, 0.0f, -halfDepth);
				glVertex3f(halfWidth, 0.0f, -halfDepth);
				glVertex3f(halfWidth, 0.0f, halfDepth);
				glVertex3f(-halfWidth, 0.0f, halfDepth);
			glEnd();

			drawDirection = true;
		}
		else if (m_areaLightType == eAreaLightDisc)
		{
			static const unsigned int kNumSegments = 20;
			const float discAngleInc = kPI * 2.0f / (float)kNumSegments;
			glBegin(GL_POLYGON);
			float Y = 0.0f;
			for (unsigned int i = 0; i < kNumSegments; i++)
			{
				float X = cosf(discAngleInc * (float)i);
				float Z = sinf(discAngleInc * (float)i);

				X *= halfWidth;
				Z *= halfDepth;

				glVertex3f(X, Y, Z);
			}

			glEnd();

			drawDirection = true;
		}

		if (drawDirection)
		{
			glBegin(GL_LINES);
				glVertex3f(0.0f, 0.0f, 0.0f);
				glVertex3f(0.0f, -1.0f, 0.0f);
				glVertex3f(0.0f, -1.0f, 0.0f);
				glVertex3f(0.0f, -0.75, 0.25);
			glEnd();
		}
	}
	else if (m_lightType == eLightPoint)
	{
		static const float kRadius = 0.5f;

		glBegin(GL_LINES);

		static const unsigned int segments = 10;

		unsigned int numParallels = segments / 2 + 1;

		float fAngleInc = 2.0f * kPI / (float)segments;

		for (unsigned int i = 0; i < numParallels; i++)
		{
			for (unsigned int j = 0; j < segments; j++)
			{
				float X = sinf(fAngleInc * (i + 1)) * cosf(fAngleInc * j);
				float Y = cosf(fAngleInc * (i + 1));
				float Z = sinf(fAngleInc * (i + 1)) * sinf(fAngleInc * j);

				X *= kRadius;
				Y *= kRadius;
				Z *= kRadius;

				glVertex3f(0.0f, 0.0f, 0.0f);
				glVertex3f(X, Y, Z);
			}
		}

		// now the last one, north to south pole
		glVertex3f(0.0f, -kRadius, 0.0f);
		glVertex3f(0.0f, kRadius, 0.0f);

		glEnd();
	}
	else if (m_lightType == eLightSpot)
	{
		// pretty hacky, and not the fastest, but...

		const float spotHeight = 1.6f;
		float angleRad = (90.0f - m_coneAngle) * (kPI / 180.0f);
		float coneRadius = spotHeight / tanf(angleRad);

		static const unsigned int kNumSegments = 20;
		const float discAngleInc = kPI * 2.0f / (float)kNumSegments;
		glBegin(GL_POLYGON);
		for (unsigned int i = 0; i < kNumSegments; i++)
		{
			// start at the top...
			glVertex3f(0.0f, 0.0f, 0.0f);

			float X0 = cosf(discAngleInc * (float)i);
			float Z0 = sinf(discAngleInc * (float)i);

			unsigned int nextI = (i < kNumSegments) ? i + 1 : 0;

			float X1 = cosf(discAngleInc * (float)nextI);
			float Z1 = sinf(discAngleInc * (float)nextI);

			X0 *= coneRadius;
			Z0 *= coneRadius;

			X1 *= coneRadius;
			Z1 *= coneRadius;

			glVertex3f(X0, -spotHeight, Z0);
			glVertex3f(X1, -spotHeight, Z1);

			glVertex3f(0.0f, 0.0f, 0.0f);
		}

		glEnd();
	}

	glPopMatrix();

	glPopAttrib();
}

DEFINE_VMP_PLUGIN(ImagineViewerModifier)

void registerPlugins()
{
	REGISTER_PLUGIN(ImagineViewerModifier, "imagineViewerModifier", 0, 1);
}
