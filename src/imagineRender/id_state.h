#ifndef ID_STATE_H
#define ID_STATE_H

#ifdef KAT_V_2
#include <FnRender/plugin/IdSenderFactory.h>
#else
#include <Render/SocketIdSender.h>
#endif

#include <unistd.h>
#include <stdio.h>

class IDState
{
public:
	IDState() : m_pIDSender(NULL), m_maxID(0), m_nextID(0)
	{
		
	}
	
	~IDState()
	{
		if (m_pIDSender)
		{
			delete m_pIDSender;
			m_pIDSender = NULL;
		}
	}
	
	bool initState(const std::string& hostName, int64_t frameID)
	{
#ifdef KAT_V_2
		m_pIDSender = FnKat::Render::IdSenderFactory::getNewInstance(hostName, frameID);
#else
		m_pIDSender = new FnKat::Render::SocketIdSender(hostName, frameID);
#endif
		
		if (!m_pIDSender)
		{
			fprintf(stderr, "Couldn't init SocketIdSender.\n");
			return false;
		}

		m_pIDSender->getIds(&m_nextID, &m_maxID);
		
		return m_maxID > 0;
	}
	
	// TODO: these are obviously not thread-safe, which for the moment is okay, but...
	
	int64_t getNextID()
	{
		if (m_nextID >= m_maxID)
		{
			return 0;
		}
		
		int64_t returnValue = m_nextID++;
		
		return returnValue;
	}
	
	void sendID(int64_t idValue, const char* locationName)
	{
		m_pIDSender->send(idValue, locationName);
	}
	
protected:
#ifdef KAT_V_2
	FnKat::Render::IdSenderInterface*	m_pIDSender;
#else
	FnKat::Render::SocketIdSender*		m_pIDSender;
#endif
	
	int64_t								m_maxID;
	int64_t								m_nextID;	
};

#endif // ID_STATE_H

