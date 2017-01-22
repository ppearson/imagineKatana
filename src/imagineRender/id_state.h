#ifndef ID_STATE_H
#define ID_STATE_H

#include <FnRender/plugin/IdSenderFactory.h>

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
		m_pIDSender = FnKat::Render::IdSenderFactory::getNewInstance(hostName, frameID);
		
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
	FnKat::Render::IdSenderInterface*	m_pIDSender;
	
	int64_t								m_maxID;
	int64_t								m_nextID;	
};

#endif // ID_STATE_H

