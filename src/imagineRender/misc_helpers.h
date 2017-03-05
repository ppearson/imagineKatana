#ifndef MISC_HELPERS_H
#define MISC_HELPERS_H

struct CreationSettings
{
	CreationSettings() : m_applyMaterials(true), m_useTextures(true), m_enableSubdivision(false), m_deduplicateVertexNormals(false),
		m_specialiseType(eNone), m_specialisedDetectInstances(true), m_useGeoNormals(true),
	    m_useBounds(true), m_motionBlur(false), m_decomposeXForms(false),
		m_discardGeometry(false),
		m_flipT(0), m_triangleType(0), m_geoQuantisationType(0), m_specialisedTriangleType(0), m_shutterOpen(0.0f), m_shutterClose(0.0f)
	{
	}

	enum SpecialiseType
	{
		eNone,
		eAssembly,
		eComponent
	};

	bool				m_applyMaterials;
	bool				m_useTextures;
	bool				m_enableSubdivision;
	bool				m_deduplicateVertexNormals;
	SpecialiseType		m_specialiseType;
	bool				m_specialisedDetectInstances;
	bool				m_useGeoNormals;
	bool				m_useBounds;

	bool				m_motionBlur;
	bool				m_decomposeXForms;
	bool				m_discardGeometry;

	unsigned int		m_flipT;
	unsigned int		m_triangleType;
	unsigned int		m_geoQuantisationType;
	unsigned int		m_specialisedTriangleType;

	float				m_shutterOpen;
	float				m_shutterClose;
};

#endif // MISC_HELPERS_H
