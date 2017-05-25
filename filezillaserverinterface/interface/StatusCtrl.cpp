// StatusCtrl.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "StatusCtrl.h"
#include "EnterSomething.h"
#include "MainFrm.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const COLORREF CStatusCtrl::m_ColTable[16] = {RGB(255, 255, 255),
										RGB(0, 0, 0),
										RGB(0, 0, 128),
										RGB(0, 128, 0),
										RGB(255, 0, 0),
										RGB(128, 0, 0),
										RGB(128, 0, 128),
										RGB(128, 128, 0),
										RGB(255, 255, 0),
										RGB(0, 255, 0),
										RGB(0, 128, 128),
										RGB(0, 255, 255),
										RGB(0, 0, 255),
										RGB(255, 0, 255),
										RGB(128, 128, 128),
										RGB(192, 192, 192)
										};

/////////////////////////////////////////////////////////////////////////////
// CStatusCtrl

CStatusCtrl::CStatusCtrl()
{
	m_doPopupCursor = FALSE;
	m_bEmpty = TRUE;
	m_nMoveToBottom = 0;
	m_nTimerID = 0;
	m_headerPos = 0;
	m_runTimer = 0;
}

CStatusCtrl::~CStatusCtrl()
{
}


BEGIN_MESSAGE_MAP(CStatusCtrl, CRichEditCtrl)
	//{{AFX_MSG_MAP(CStatusCtrl)
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_WM_SETCURSOR()
	ON_COMMAND(ID_OUTPUTCONTEXT_CLEARALL, OnOutputcontextClearall)
	ON_COMMAND(ID_OUTPUTCONTEXT_COPYTOCLIPBOARD, OnOutputcontextCopytoclipboard)
	ON_WM_CREATE()
	ON_WM_TIMER()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEWHEEL()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten CStatusCtrl

BOOL CStatusCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

void CStatusCtrl::OnContextMenu(CWnd* pWnd, CPoint point)
{
	ClientToScreen(&point);

	CMenu menu;
	menu.LoadMenu(IDR_OUTPUTCONTEXT);

	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup != NULL);
	CWnd* pWndPopupOwner = this;
	//while (pWndPopupOwner->GetStyle() & WS_CHILD)
	//	pWndPopupOwner = pWndPopupOwner->GetParent();

	if (!GetLineCount())
	{
		pPopup->EnableMenuItem(ID_OUTPUTCONTEXT_COPYTOCLIPBOARD,MF_GRAYED);
		pPopup->EnableMenuItem(ID_OUTPUTCONTEXT_CLEARALL,MF_GRAYED);
	}
	HCURSOR	hCursor;
	hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	m_doPopupCursor = TRUE;
	SetCursor(hCursor);

	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
		pWndPopupOwner);
}

BOOL CStatusCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (!m_doPopupCursor)
	{
		m_doPopupCursor = 0;
		return CWnd::OnSetCursor(pWnd, nHitTest, message );
	}
	else
		m_doPopupCursor = 0;
	return 0;
}

DWORD __stdcall CStatusCtrl::RichEditStreamInCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
	char* output = (char*)pbBuff;

	CStatusCtrl *pThis = (CStatusCtrl *)dwCookie;
	if (pThis->m_headerPos != -1)
	{
		int len = pThis->m_RTFHeader.GetLength() - pThis->m_headerPos;
		if (len > cb)
		{
			pThis->m_headerPos = cb;
			len = cb;
		}
		else
			pThis->m_headerPos = -1;

		memcpy(output, (const char*)pThis->m_RTFHeader, len);
		*pcb = len;
	}
	else
	{
		*pcb = 0;
		if (pThis->m_statusBuffer.empty())
			return 0;
		t_buffer &buffer = pThis->m_statusBuffer.front();
		if (buffer.status != _T(""))
		{
			if (buffer.pos == -1)
			{
				if (pThis->m_bEmpty)
				{
					pThis->m_bEmpty = false;
					memcpy(output, "\\cf", 3);
					output += 3;
					cb -= 3;
					*pcb += 3;
				}
				else
				{
					memcpy(output, "\\par \\cf", 8);
					output += 8;
					cb -= 8;
					*pcb += 8;
				}
				switch (buffer.type)
				{
				default:
				case 0:
					*(output++) = '2';
					break;
				case 1:
					*(output++) = '5';
					break;
				case 2:
					*(output++) = '3';
					break;
				case 3:
					*(output++) = '4';
					break;
				}
				buffer.pos = 0;
			}
			LPCTSTR status = buffer.status;
			LPCTSTR p = status + buffer.pos;
			while (*p && cb > 9)
			{
				switch (*p)
				{
				case '\\':
					*(output++) = '\\';
					*(output++) = '\\';
					cb -= 2;
					*pcb += 2;
					break;
				case '{':
					*(output++) = '\\';
					*(output++) = '{';
					cb -= 2;
					*pcb += 2;
					break;
				case '}':
					*(output++) = '\\';
					*(output++) = '}';
					cb -= 2;
					*pcb += 2;
					break;
				case '\r':
					break;
				case '\n':
					*(output++) = '\\';
					*(output++) = 's';
					*(output++) = 't';
					*(output++) = 'a';
					*(output++) = 't';
					*(output++) = 'u';
					*(output++) = 's';
					cb -= 7;
					*pcb += 7;
					break;
				default:
					if (*p > 127)
					{
						int w = sprintf(output, "\\u%d?", (unsigned short)*p);
						output += w;
						cb -= w;
						*pcb += w;
					}
					else
					{
						*(output++) = (char)*p;
						cb--;
						(*pcb)++;
					}
				}
				p++;

			}
			if (!*p)
			{
				pThis->m_statusBuffer.pop_front();
				if (pThis->m_statusBuffer.empty())
				{
					memcpy(output, "} ", 2);
					output += 2;
					*pcb += 2;
				}
				else
				{
					*(output++) = ' ';
					(*pcb)++;
				}
			}
			else
				buffer.pos = p - status;
		}
		else
		{
			pThis->m_statusBuffer.pop_front();
			if (pThis->m_statusBuffer.empty())
			{
				memcpy(output, "} ", 2);
				output += 2;
				*pcb += 2;
			}
		}
	}

	return 0;
}

void CStatusCtrl::OnOutputcontextClearall()
{
	t_buffer buffer;
	buffer.status = _T("");
	buffer.pos = -1;
	buffer.type = 0;
	m_statusBuffer.push_back(buffer);

	DoStreamIn();

	SetSel(-1, -1);
	LimitText(1000*1000);

	m_bEmpty = TRUE;
	m_nMoveToBottom = 0;
}

void CStatusCtrl::OnOutputcontextCopytoclipboard()
{
	long nStart, nEnd;
	GetSel(nStart, nEnd);
	if (nStart == nEnd)
	{
		HideSelection(TRUE, FALSE);
		SetSel(0, -1);
		Copy();
		SetSel(nStart, nEnd);
		HideSelection(FALSE, FALSE);
	}
	else
		Copy();
}


int CStatusCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CRichEditCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;

	USES_CONVERSION;

	m_RTFHeader = "{\\rtf1\\ansi\\deff0";

	HFONT hSysFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	LOGFONT lf;
	CFont* pFont = CFont::FromHandle( hSysFont );
	pFont->GetLogFont( &lf );

	LOGFONT m_lfFont;
	pFont->GetLogFont(&m_lfFont);

	m_RTFHeader += "{\\fonttbl{\\f0\\fnil "+ CString(m_lfFont.lfFaceName)+";}}";
	m_RTFHeader += "{\\colortbl ;";
	for (int i=0; i<16; i++)
	{
		CString tmp;
		tmp.Format(_T("\\red%d\\green%d\\blue%d;"), GetRValue(m_ColTable[i]), GetGValue(m_ColTable[i]), GetBValue(m_ColTable[i]));
		m_RTFHeader+=tmp;
	}
	m_RTFHeader += "}";

	int pointsize = (-m_lfFont.lfHeight*72/ GetDeviceCaps(GetDC()->GetSafeHdc(), LOGPIXELSY))*2;
	CString tmp;
	tmp.Format(_T("%d"), pointsize);
	m_RTFHeader += "\\uc1\\pard\\fi-200\\li200\\tx200\\f0\\fs"+tmp; //180*m_nAvgCharWidth;

	t_buffer buffer;
	buffer.status = _T("");
	buffer.pos = -1;
	buffer.type = 0;
	m_statusBuffer.push_back(buffer);

	m_headerPos = 0;

	DoStreamIn();

	SetSel(-1, -1);
	LimitText(1000*1000);

	return 0;
}

void CStatusCtrl::ShowStatus(LPCTSTR status, int nType)
{
	t_buffer buffer;
	buffer.status = status;
	buffer.type = nType;
	buffer.pos = -1;

	m_statusBuffer.push_back(buffer);

	if (!m_runTimer)
	{
		Run();
		m_runTimer = SetTimer(1339, 250, 0);
	}

	return;
}

void CStatusCtrl::Run()
{
	if (m_statusBuffer.empty())
		return;

	m_headerPos = 0;

	CWnd *pFocusWnd = GetFocus();
	if (pFocusWnd && pFocusWnd == this)
		AfxGetMainWnd()->SetFocus();

	long nStart, nEnd;
	GetSel(nStart, nEnd);
	BOOL nScrollToEnd = FALSE;

	int num = 0;			//this is the number of visible lines
	CRect rect;
	GetRect(rect);
	int height = rect.Height();

	for (int i = GetFirstVisibleLine();
			i < GetLineCount() && GetCharPos(LineIndex(i)).y < height;
			i++)
		num++;

	if (GetFirstVisibleLine() + num+m_nMoveToBottom >= GetLineCount())
		nScrollToEnd = TRUE;
	HideSelection(TRUE, FALSE);
	SetSel(-1, -1);

	DoStreamIn(SFF_SELECTION);

	int count = GetLineCount();
	if (count > 1000)
	{
		count = count - 1000;
		int index = LineIndex(count);
		nStart -= index;
		nEnd -= index;
		if (nStart < 0)
			nEnd = 0;
		if (nEnd < 0)
			nEnd = 0;
		SetSel(0, index);
		ReplaceSel(_T(""));
	}

	SetSel(nStart, nEnd);

	if (pFocusWnd && pFocusWnd == this)
		SetFocus();

	HideSelection(FALSE, FALSE);
	if (nScrollToEnd)
	{
		if (nStart != nEnd && (LineFromChar(nStart) >= GetFirstVisibleLine() && LineFromChar(nStart) <= GetFirstVisibleLine() + num ||
							   LineFromChar(nEnd) >= GetFirstVisibleLine() && LineFromChar(nEnd) <= GetFirstVisibleLine() + num))
			LineScroll(1);
		else
		{
			m_nMoveToBottom++;
			if (!m_nTimerID)
				m_nTimerID = SetTimer(654, 25, NULL);
		}
	}
}

void CStatusCtrl::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == m_nTimerID)
	{
		if (m_nMoveToBottom)
		{
			SendMessage(WM_VSCROLL, SB_BOTTOM, 0);
			m_nMoveToBottom = 0;
		}
		KillTimer(m_nTimerID);
		m_nTimerID = 0;
	}
	else if (nIDEvent == static_cast<UINT>(m_runTimer))
	{
		KillTimer(m_runTimer);
		m_runTimer = 0;
		Run();
	}
	CRichEditCtrl::OnTimer(nIDEvent);
}

void CStatusCtrl::OnRButtonUp(UINT nFlags, CPoint point)
{
	ClientToScreen(&point);

	CMenu menu;
	menu.LoadMenu(IDR_OUTPUTCONTEXT);

	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup != NULL);
	CWnd* pWndPopupOwner = this;
	//while (pWndPopupOwner->GetStyle() & WS_CHILD)
	//	pWndPopupOwner = pWndPopupOwner->GetParent();

	if (!GetLineCount())
	{
		pPopup->EnableMenuItem(ID_OUTPUTCONTEXT_COPYTOCLIPBOARD,MF_GRAYED);
		pPopup->EnableMenuItem(ID_OUTPUTCONTEXT_CLEARALL,MF_GRAYED);
	}
	HCURSOR	hCursor;
	hCursor = AfxGetApp()->LoadStandardCursor( IDC_ARROW );
	m_doPopupCursor = TRUE;
	SetCursor(hCursor);

	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
		pWndPopupOwner);
}

BOOL CStatusCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	OSVERSIONINFO info = {0};
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&info);
	if (info.dwMajorVersion >= 5)
		return CRichEditCtrl::OnMouseWheel(nFlags, zDelta, pt);

	LineScroll(-zDelta / 120 * 3);

	return TRUE;
}

void CStatusCtrl::DoStreamIn(int extraFlags)
{
	EDITSTREAM es;
	es.dwCookie = (DWORD)this; // Pass a pointer to the CString to the callback function
	es.pfnCallback = RichEditStreamInCallback; // Specify the pointer to the callback function.

	StreamIn(extraFlags | SF_RTF, es); // Perform the streaming
}
