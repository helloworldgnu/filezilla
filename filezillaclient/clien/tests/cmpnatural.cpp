#include <libfilezilla.h>
#include <wx/imaglist.h>
#include <wx/scrolwin.h>
#include <wx/listctrl.h>
#include <../interface/filelistctrl.h>

#include <cppunit/extensions/HelperMacros.h>
#include <list>

/*
 * This testsuite asserts the correctness of the
 * functions handling natural sort
 */

class CNaturalSortTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(CNaturalSortTest);
	CPPUNIT_TEST(testEmpty);
	CPPUNIT_TEST(testCaseInsensitive);
	CPPUNIT_TEST(testString);
	CPPUNIT_TEST(testNumber);
	CPPUNIT_TEST(testMixed);
	CPPUNIT_TEST(testSeq);
	CPPUNIT_TEST(testPair);
	CPPUNIT_TEST(testFractional);
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp() {}
	void tearDown() {}

	void testEmpty();
	void testCaseInsensitive();
	void testString();
	void testNumber();
	void testMixed();
	void testSeq();
	void testPair();
	void testFractional();
};

CPPUNIT_TEST_SUITE_REGISTRATION(CNaturalSortTest);

void CNaturalSortTest::testEmpty()
{
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T(""), _T("")) == 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T(""), _T("x")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("x"), _T("")) > 0);
}

void CNaturalSortTest::testCaseInsensitive()
{
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a"), _T("A")) == 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("B"), _T("b")) == 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a"), _T("B")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("A"), _T("b")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("afFasFAc"), _T("aFfaSFaC")) == 0);
}

void CNaturalSortTest::testString()
{
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a"), _T("b")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("b"), _T("a")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a"), _T("ab")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("ab"), _T("a")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("affasfac"), _T("affasfac")) == 0);
}

void CNaturalSortTest::testNumber()
{
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1"), _T("1")) == 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1"), _T("2")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("2"), _T("1")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("15"), _T("25")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("25"), _T("15")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("15"), _T("17")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("17"), _T("15")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1"), _T("10")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("10"), _T("1")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("2"), _T("17")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("17"), _T("2")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("2"), _T("02")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("02"), _T("2")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("02"), _T("1")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1"), _T("02")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("02"), _T("3")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("3"), _T("02")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("25"), _T("021")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("021"), _T("25")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("2100"), _T("02005")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("02005"), _T("2100")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("010"), _T("02")) > 0);
}

void CNaturalSortTest::testMixed()
{
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("0"), _T("a")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a"), _T("0")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("abc1xx"), _T("abc2xx")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("abc1bb"), _T("abc2aa")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("abc2"), _T("1")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("10abc"), _T("10def")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("10def"), _T("10abc")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("10abc2"), _T("10abc3")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("10abc3"), _T("10abc2")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("10abc"), _T("10abc3")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("10abc3"), _T("10abc")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1"), _T("1abc")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1abc"), _T("1")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("2"), _T("1abc")) > 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1abc"), _T("2")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1def"), _T("10abc")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("10abc"), _T("1def")) > 0);
}

void CNaturalSortTest::testSeq()
{
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a"), _T("a0")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a0"), _T("a1")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a1"), _T("a1a")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a1a"), _T("a1b")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a1b"), _T("a2")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a2"), _T("a10")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("a10"), _T("a20")) < 0);
}

void CNaturalSortTest::testPair()
{
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("x2-g8"), _T("x2-y7")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("x2-y7"), _T("x2-y08")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("x2-y08"), _T("x8-y8")) < 0);
}

void CNaturalSortTest::testFractional()
{
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1.001"), _T("1.002")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1.002"), _T("1.010")) < 0);
	//CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1.010"), _T("1.02")) < 0);   //this fraction case would break 010 > 02
	//CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1.02"), _T("1.1")) < 0);     //this fraction case would break 02 > 1
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1.1"), _T("1.3")) < 0);
	CPPUNIT_ASSERT(CFileListCtrlSortBase::CmpNatural(_T("1.3"), _T("1.15")) < 0);
}
