#if !defined(AFX_STATUSCTRL_H__CBCF375F_27B2_4A35_B7CE_BD0893925945__INCLUDED_)
#define AFX_STATUSCTRL_H__CBCF375F_27B2_4A35_B7CE_BD0893925945__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// StatusCtrl.h : Header-Datei
//

// -----------------------------------------------------------------
// Colour Codes:
// -----------------------------------------------------------------


#define COLOUR_WHITE		0
#define COLOUR_BLACK		1
#define COLOUR_BLUE			2
#define COLOUR_GREEN		3
#define COLOUR_LIGHTRED		4
#define COLOUR_BROWN		5
#define COLOUR_PURPLE		6
#define COLOUR_ORANGE		7
#define COLOUR_YELLOW		8
#define COLOUR_LIGHTGREEN	9
#define COLOUR_CYAN			10
#define COLOUR_LIGHTCYAN	11
#define COLOUR_LIGHTBLUE	12
#define COLOUR_PINK			13
#define COLOUR_GREY			14
#define COLOUR_LIGHTGREY	15

/////////////////////////////////////////////////////////////////////////////
// Fenster CStatusCtrl

class CStatusView;
class CStatusCtrl : public CRichEditCtrl
{
	friend class CStatusView;

public:
	CStatusCtrl();
	virtual ~CStatusCtrl();

	void ShowStatus(LPCTSTR status, int nType);
	void Run();

//	virtual BOOL Create( DWORD in_dwStyle, const RECT& in_rcRect,
//						 CWnd* in_pParentWnd, UINT in_nID );

	// Generierte Nachrichtenzuordnungsfunktionen
protected:

	void DoStreamIn(int extraFlags = 0);
	static DWORD __stdcall RichEditStreamInCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb);

	struct t_buffer
	{
		CString status;
		int pos;
		int type;
	};

	std::list<t_buffer> m_statusBuffer;
	int m_headerPos;

	BOOL m_bEmpty;
	UINT_PTR m_nTimerID;
	int m_runTimer;
	int m_nMoveToBottom;

	BOOL m_doPopupCursor;
	const static COLORREF m_ColTable[16]; // Colour Table

	CStringA m_RTFHeader;

	//{{AFX_MSG(CStatusCtrl)
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnOutputcontextClearall();
	afx_msg void OnOutputcontextCopytoclipboard();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/*class _AFX_RICHEDITEX_STATE
{
public:
	_AFX_RICHEDITEX_STATE();
	virtual ~_AFX_RICHEDITEX_STATE();

	HINSTANCE m_hInstRichEdit20 ;
};

BOOL PASCAL AfxInitRichEditEx();
*/

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ fügt unmittelbar vor der vorhergehenden Zeile zusätzliche Deklarationen ein.

#endif // AFX_STATUSCTRL_H__CBCF375F_27B2_4A35_B7CE_BD0893925945__INCLUDED_
