#ifndef LIVE_RENDER_HELPERS_H
#define LIVE_RENDER_HELPERS_H

#include <string>
#include <vector>

#include "core/hash.h"
#include "colour/colour3f.h"
#include "utils/threads/mutex.h"

namespace Imagine
{
class Material;
}

struct KatanaUpdateItem
{
	enum UpdateLocationType
	{
		eLocUnknown,
		eLocCamera,
		eLocObject,
		eLocLight
	};
	
	enum UpdateType
	{
		eTypeUnknown,
		eTypeCamera,
		eTypeObject,
		eTypeObjectMaterial,
		eTypeLight
	};
	
	// pretty disgusting - only for prototype purposes - need full Args stuff here.
	struct MaterialDef
	{
		Imagine::Colour3f		diffColour;
	};
	
	KatanaUpdateItem(UpdateType ty, UpdateLocationType locType, const std::string& loc) : type(ty), locationType(locType), location(loc),
									pMaterial(NULL)
	{
		
	}	

	UpdateType				type;
	UpdateLocationType		locationType;
	std::string				location;
	
//	MaterialDef				material;
	Imagine::Material*		pMaterial;
	
	std::vector<double>		xform;
};


class LiveRenderState
{
public:
	LiveRenderState() : m_lastCameraTransformHash(0LL)
	{

	}

	bool hasUpdates() const
	{
		// really needs a lock, but let's see how we get on without one...
		// TODO: use an atomic value to indicate this, which is updated by below functions...
		return !m_aUpdateItems.empty();
	}

	void addUpdate(const KatanaUpdateItem& updateItem)
	{
		m_updateLock.lock();
		m_aUpdateItems.push_back(updateItem);
		m_updateLock.unlock();
	}

	void flushUpdates()
	{
		m_updateLock.lock();
		m_aUpdateItems.clear();
		m_updateLock.unlock();
	}

	void lock()
	{
		m_updateLock.lock();
	}

	void unlock()
	{
		m_updateLock.unlock();
	}
	
	

	std::vector<KatanaUpdateItem>::const_iterator updatesBegin() const { return m_aUpdateItems.begin(); }
	std::vector<KatanaUpdateItem>::const_iterator updatesEnd() const { return m_aUpdateItems.end(); }

protected:
	// for live render updates
	Imagine::Mutex					m_updateLock;
	std::vector<KatanaUpdateItem>	m_aUpdateItems;
	
	Imagine::HashValue				m_lastCameraTransformHash;
};

#endif // LIVE_RENDER_HELPERS_H
