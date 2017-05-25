#ifndef FILEZILLA_XRC_HELPER_HEADER
#define FILEZILLA_XRC_HELPER_HEADER

/*
xrc_call is a safer and simpler alternative to XRCCTRL:
- uses dynamic_cast
- Returns default-contructed return value if object does not exist
- Asserts in debug if object does not exist

Usage:
  bool always = xrc_call(*this, "ID_ALWAYS", &wxCheckBox::GetValue);

  Instead of this:
  bool always = XRCCTRL(*this, "ID_ALWAYS", wxCheckBox)->GetValue();

*/

template<typename F, typename Control, typename ...Args, typename ...Args2>
F xrc_call(wxWindow& parent, char const* name, F(Control::* ptr)(Args...), Args2&& ... args)
{
	F ret{};
	Control* c = dynamic_cast<Control*>(parent.FindWindow(XRCID(name)));
	wxASSERT(c);
	if (c) {
		ret = (c->*ptr)(std::forward<Args2>(args)...);
	}

	return ret;
}

template<typename Control, typename ...Args, typename ...Args2>
void xrc_call(wxWindow& parent, char const* name, void (Control::* ptr)(Args...), Args2&& ... args)
{
	Control* c = dynamic_cast<Control*>(parent.FindWindow(XRCID(name)));
	wxASSERT(c);
	if (c) {
		(c->*ptr)(std::forward<Args2>(args)...);
	}
}

template<typename S, typename F, typename Control, typename ...Args, typename ...Args2>
F xrc_call(wxWindow& parent, S&& name, F(Control::* ptr)(Args...) const, Args2&& ... args)
{
	F ret{};
	Control* c = dynamic_cast<Control*>(parent.FindWindow(XRCID(name)));
	wxASSERT(c);
	if (c) {
		ret = (c->*ptr)(std::forward<Args2>(args)...);
	}

	return ret;
}

template<typename S, typename Control, typename ...Args, typename ...Args2>
void xrc_call(wxWindow& parent, S&& name, void (Control::* ptr)(Args...) const, Args2&& ... args)
{
	Control* c = dynamic_cast<Control*>(parent.FindWindow(XRCID(name)));
	wxASSERT(c);
	if (c) {
		(c->*ptr)(std::forward<Args2>(args)...);
	}
}

#endif