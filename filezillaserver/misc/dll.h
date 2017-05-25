#ifndef FZS_DLL_HEADER
#define FZS_DLL_HEADER

#ifndef _AFX
#define CString CStdString
#endif

#include <vector>

class DLL final
{
public:
	DLL();
	explicit DLL(CString const& s);
	~DLL();

	DLL(DLL const&) = delete;
	DLL& operator=(DLL const&) = delete;

	bool load(CString const& s);

	void clear();
	
	void* load_func(LPCSTR name, void** out);

	explicit operator bool() const { return hModule != 0; }

	HMODULE get() { return hModule; }
protected:
	std::vector<void**> loaded_functions;
	HMODULE hModule{};
};

#endif