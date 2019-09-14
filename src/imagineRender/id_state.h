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

