#ifndef __OPTION_CHANGE_EVENT_HANDLER_H__
#define __OPTION_CHANGE_EVENT_HANDLER_H__

#include <bitset>
#include <set>
#include <vector>

class COptions;

enum {
	changed_options_size = 128
};

typedef std::bitset<changed_options_size> changed_options_t;

class COptionChangeEventHandler
{
	friend class COptions;

public:
	COptionChangeEventHandler();
	virtual ~COptionChangeEventHandler();

	void RegisterOption(int option);
	void UnregisterOption(int option);

protected:
	virtual void OnOptionsChanged(changed_options_t const& options) = 0;

private:
	changed_options_t m_handled_options;

	static void DoNotify(changed_options_t const& options);
	static void UnregisterAll();

	static std::vector<COptionChangeEventHandler*> m_handlers;
};

#endif //__OPTION_CHANGE_EVENT_HANDLER_H__
