#include <libfilezilla.h>

#include <event_handler.h>
#include <event_loop.h>

#include <cppunit/extensions/HelperMacros.h>

/*
 * This testsuite asserts the correctness of the
 * functions handling natural sort
 */

class EventloopTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(EventloopTest);
	CPPUNIT_TEST(testSimple);
	CPPUNIT_TEST(testFilter);
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp() {}
	void tearDown() {}

	void testSimple();
	void testFilter();
};

CPPUNIT_TEST_SUITE_REGISTRATION(EventloopTest);

namespace {
struct type1;
typedef CEvent<type1> T1;

struct type2;
typedef CEvent<type2, int> T2;

struct type3;
typedef CEvent<type3> T3;

struct type4;
typedef CEvent<type4> T4;

class target : public CEventHandler
{
public:
	target(CEventLoop & l)
	: CEventHandler(l)
	{}

	virtual ~target()
	{
		RemoveHandler();
	}

	void a()
	{
		++a_;
		SendEvent<T2>(5);
	}

	void b(int v)
	{
		++b_;

		CPPUNIT_ASSERT_EQUAL(v, 5);
	}

	void c()
	{
		SendEvent<T4>();
	}

	void d()
	{
		scoped_lock l(m_);
		cond_.signal(l);
	}

	virtual void operator()(CEventBase const& ev) override {
		CPPUNIT_ASSERT((Dispatch<T1, T2, T3, T4>(ev, this, &target::a, &target::b, &target::c, &target::d)));
	}

	int a_{};
	int b_{};


	mutex m_;
	condition cond_;
};
}

void EventloopTest::testSimple()
{
	CEventLoop loop;

	target t(loop);

	for (int i = 0; i < 1000; ++i) {
		t.SendEvent<T1>();
	}

	t.SendEvent<T3>();

	scoped_lock l(t.m_);
	CPPUNIT_ASSERT(t.cond_.wait(l, 1000));

	CPPUNIT_ASSERT_EQUAL(t.a_, 1000);
	CPPUNIT_ASSERT_EQUAL(t.b_, 1000);
}

namespace {
class target2 : public CEventHandler
{
public:
	target2(CEventLoop & l)
	: CEventHandler(l)
	{}

	virtual ~target2()
	{
		RemoveHandler();
	}

	void a()
	{
		{
			scoped_lock l(m_);
			CPPUNIT_ASSERT(cond2_.wait(l, 1000));
		}

		auto f = [&](CEventLoop::Events::value_type& ev) -> bool {
			if (ev.second->derived_type() == T1::type()) {
				++c_;
				return true;
			}

			if (ev.second->derived_type() == T2::type()) {
				++d_;
				std::get<0>(static_cast<T2&>(*ev.second).v_) += 4;
			}
			return false;

		};
		event_loop_.FilterEvents(f);
		++a_;
	}

	void b(int v)
	{
		b_ += v;
	}

	void c()
	{
		scoped_lock l(m_);
		cond_.signal(l);
	}

	virtual void operator()(CEventBase const& ev) override {
		CPPUNIT_ASSERT((Dispatch<T1, T2, T3>(ev, this, &target2::a, &target2::b, &target2::c)));
	}

	int a_{};
	int b_{};
	int c_{};
	int d_{};

	mutex m_;
	condition cond_;
	condition cond2_;
};
}

void EventloopTest::testFilter()
{
	CEventLoop loop;

	target2 t(loop);

	for (int i = 0; i < 10; ++i) {
		t.SendEvent<T1>();
	}
	t.SendEvent<T2>(3);
	t.SendEvent<T2>(5);

	t.SendEvent<T3>();

	scoped_lock l(t.m_);
	t.cond2_.signal(l);

	CPPUNIT_ASSERT(t.cond_.wait(l, 1000));

	CPPUNIT_ASSERT_EQUAL(t.a_, 1);
	CPPUNIT_ASSERT_EQUAL(t.b_, 16);
	CPPUNIT_ASSERT_EQUAL(t.c_, 9);
	CPPUNIT_ASSERT_EQUAL(t.d_, 2);
}
