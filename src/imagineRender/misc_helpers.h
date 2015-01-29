#ifndef MISC_HELPERS_H
#define MISC_HELPERS_H

struct CreationSettings
{
	CreationSettings() : m_applyMaterials(true), m_useTextures(true), m_enableSubdivision(false), m_deduplicateVertexNormals(false),
		m_specialiseAssemblies(false), m_useGeoNormals(true), m_motionBlur(false), m_flipT(0), m_triangleType(0),
		m_geoQuantisationType(0), m_specialisedTriangleType(0)
	{
	}

	bool				m_applyMaterials;
	bool				m_useTextures;
	bool				m_enableSubdivision;
	bool				m_deduplicateVertexNormals;
	bool				m_specialiseAssemblies;
	bool				m_useGeoNormals;

	bool				m_motionBlur;

	unsigned int		m_flipT;
	unsigned int		m_triangleType;
	unsigned int		m_geoQuantisationType;
	unsigned int		m_specialisedTriangleType;
};

#endif // MISC_HELPERS_H
