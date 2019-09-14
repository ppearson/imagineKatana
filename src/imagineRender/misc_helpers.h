/*
 ImagineKatana
 Copyright 2014-2019 Peter Pearson.

 Licensed under the Apache License, Version 2.0 (the "License");
 You may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ---------
*/

#ifndef MISC_HELPERS_H
#define MISC_HELPERS_H

struct CreationSettings
{
	CreationSettings() : m_applyMaterials(true), m_useTextures(true), m_enableSubdivision(false), m_deduplicateVertexNormals(false),
		m_specialiseType(eNone), m_specialisedDetectInstances(true), m_useGeoNormals(true),
	    m_useBounds(true), m_followRelativeInstanceSources(true), m_motionBlur(false), m_decomposeXForms(false),
		m_discardGeometry(false), m_chunkedParallelBuild(false),
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
	bool				m_followRelativeInstanceSources;

	bool				m_motionBlur;
	bool				m_decomposeXForms;
	bool				m_discardGeometry;
	bool				m_chunkedParallelBuild;

	unsigned int		m_flipT;
	unsigned int		m_triangleType;
	unsigned int		m_geoQuantisationType;
	unsigned int		m_specialisedTriangleType;

	float				m_shutterOpen;
	float				m_shutterClose;
};

#endif // MISC_HELPERS_H
