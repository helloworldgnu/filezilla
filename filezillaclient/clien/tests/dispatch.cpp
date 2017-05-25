#include <libfilezilla.h>

#include <event_loop.h>

#include <cppunit/extensions/HelperMacros.h>

/*
 * This testsuite asserts the correctness of the
 * functions handling natural sort
 */

class DispatchTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(DispatchTest);
	CPPUNIT_TEST(testSingle);
	CPPUNIT_TEST(testArgs);
	CPPUNIT_TEST(testMultiple);
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp() {}
	void tearDown() {}

	void testSingle();
	void testArgs();
	void testMultiple();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DispatchTest);

namespace {
struct target {

	void a() { ++a_; }
	void b() { ++b_; }
	void c() { ++c_; }

	void two(int a, int b)
	{
		a_ += a;
		b_ += b;
	}

	int a_{};
	int b_{};
	int c_{};
};

struct type1;
typedef CEvent<type1> T1;

struct type2;
typedef CEvent<type2> T2;

struct type3;
typedef CEvent<type3> T3;

struct type4;
typedef CEvent<type4, int, int> T4;
}

void DispatchTest::testSingle()
{
	target t;

	T1 const t1;
	CPPUNIT_ASSERT(Dispatch<T1>(t1, &t, &target::a));
	CPPUNIT_ASSERT(Dispatch<T1>(t1, &t, &target::b));

	T2 const t2;
	CPPUNIT_ASSERT(!Dispatch<T1>(t2, &t, &target::b));

	CPPUNIT_ASSERT_EQUAL(t.a_, 1);
	CPPUNIT_ASSERT_EQUAL(t.b_, 1);
}

void DispatchTest::testArgs()
{
	target t;

	T4 const t4(1, 5);
	CPPUNIT_ASSERT(Dispatch<T4>(t4, &t, &target::two));

	T3 const t3;
	CPPUNIT_ASSERT(!Dispatch<T4>(t3, &t, &target::two));

	CPPUNIT_ASSERT_EQUAL(t.a_, 1);
	CPPUNIT_ASSERT_EQUAL(t.b_, 5);
}

void DispatchTest::testMultiple()
{
	target t;

	T1 const t1;
	T2 const t2;
	T3 const t3;
	T4 const t4(3, 8);

	CPPUNIT_ASSERT((Dispatch<T1, T2, T3>(t1, &t, &target::a, &target::b, &target::c)));
	CPPUNIT_ASSERT((Dispatch<T1, T2, T3>(t2, &t, &target::a, &target::b, &target::c)));
	CPPUNIT_ASSERT((Dispatch<T1, T2, T3>(t3, &t, &target::a, &target::b, &target::c)));
	CPPUNIT_ASSERT((!Dispatch<T1, T2, T3>(t4, &t, &target::a, &target::b, &target::c)));

	CPPUNIT_ASSERT_EQUAL(t.a_, 1);
	CPPUNIT_ASSERT_EQUAL(t.b_, 1);
	CPPUNIT_ASSERT_EQUAL(t.c_, 1);

	CPPUNIT_ASSERT((Dispatch<T1, T4>(t4, &t, &target::a, &target::two)));

	CPPUNIT_ASSERT_EQUAL(t.a_, 4);
	CPPUNIT_ASSERT_EQUAL(t.b_, 9);
}
