#include <filezilla.h>
#include "option_change_event_handler.h"

#include <algorithm>

std::vector<COptionChangeEventHandler*> COptionChangeEventHandler::m_handlers;

COptionChangeEventHandler::COptionChangeEventHandler()
{
}

COptionChangeEventHandler::~COptionChangeEventHandler()
{
	if (m_handled_options.any()) {
		auto it = std::find(m_handlers.begin(), m_handlers.end(), this);
		if (it != m_handlers.end()) {
			m_handlers.erase(it);
		}
	}
}

void COptionChangeEventHandler::RegisterOption(int option)
{
	if (option < 0 )
		return;

	if (m_handled_options.none()) {
		m_handlers.push_back(this);
	}
	m_handled_options.set(option);
}

void COptionChangeEventHandler::UnregisterOption(int option)
{
	m_handled_options.set(option, false);
	if (m_handled_options.none()) {
		auto it = std::find(m_handlers.begin(), m_handlers.end(), this);
		if (it != m_handlers.end()) {
			m_handlers.erase(it);
		}
	}
}

void COptionChangeEventHandler::UnregisterAll()
{
	for (auto & handler : m_handlers) {
		handler->m_handled_options.reset();
	}
	m_handlers.clear();
}

void COptionChangeEventHandler::DoNotify(changed_options_t const& options)
{
	for (auto & handler : m_handlers) {
		auto hoptions = options & handler->m_handled_options;
		if (hoptions.any()) {
			handler->OnOptionsChanged(hoptions);
		}
	}
}
