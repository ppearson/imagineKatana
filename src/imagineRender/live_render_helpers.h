#ifndef LIVE_RENDER_HELPERS_H
#define LIVE_RENDER_HELPERS_H

#include <string>
#include <vector>

#include "utils/threads/mutex.h"

struct KatanaUpdateItem
{
	bool		camera;
	std::vector<double>	xform;
};


class LiveRenderState
{
public:
	LiveRenderState()
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
};

#endif // LIVE_RENDER_HELPERS_H
