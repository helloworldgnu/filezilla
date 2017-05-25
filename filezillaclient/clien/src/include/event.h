#ifndef FILEZILLA_ENGINE_EVENT_HEADER
#define FILEZILLA_ENGINE_EVENT_HEADER

class CEventBase
{
public:
	CEventBase() = default;
	virtual ~CEventBase() {}

	CEventBase(CEventBase const&) = delete;
	CEventBase& operator=(CEventBase const&) = delete;

	virtual void const* derived_type() const = 0;
};

template<typename UniqueType, typename...Values>
class CEvent final : public CEventBase
{
public:
	typedef UniqueType unique_type;
	typedef std::tuple<Values...> tuple_type;

	CEvent() = default;

	template<typename First_Value, typename...Remaining_Values>
	explicit CEvent(First_Value&& value, Remaining_Values&& ...values)
		: v_(std::forward<First_Value>(value), std::forward<Remaining_Values>(values)...)
	{
	}

	CEvent(CEvent const& op) = default;
	CEvent& operator=(CEvent const& op) = default;

	static void const* type() {
		static const char* f = 0;
		return &f;
	}

	virtual void const* derived_type() const {
		return type();
	}

	tuple_type v_;
};

template<typename T>
bool same_type(CEventBase const& ev)
{
	return ev.derived_type() == T::type();
}

typedef unsigned long long timer_id;
struct timer_event_type{};
typedef CEvent<timer_event_type, timer_id> CTimerEvent;

#endif
