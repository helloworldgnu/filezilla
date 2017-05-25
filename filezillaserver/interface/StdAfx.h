// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2016 - Tim Kosse <tim.kosse@filezilla-project.org>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// stdafx.h : Include-Datei für Standard-System-Include-Dateien,
//  oder projektspezifische Include-Dateien, die häufig benutzt, aber
//      in unregelmäßigen Abständen geändert werden.
//

#if !defined(AFX_STDAFX_H__0D7D6CEC_E1AA_4287_BB10_A97FA4D444B6__INCLUDED_)
#define AFX_STDAFX_H__0D7D6CEC_E1AA_4287_BB10_A97FA4D444B6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN		// Selten verwendete Teile der Windows-Header nicht einbinden

#pragma warning (disable : 4786)

//Added by zhangyl 2017.01.20
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

//#define _WIN32_WINNT 0x0601

#define NOMINMAX

//added by zhangyl2017.01.20
//#include <atlstr.h>

#include <afxwin.h>			// MFC-Kern- und -Standardkomponenten
#include <afxext.h>			// MFC-Erweiterungen
#include <afxdisp.h>		// MFC Automatisierungsklassen
#include <afxdtctl.h>		// MFC-Unterstützung für allgemeine Steuerelemente von Internet Explorer 4
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC-Unterstützung für gängige Windows-Steuerelemente
#endif // _AFX_NO_AFXCMN_SUPPORT

//#include <afxsock.h>		// MFC-Socket-Erweiterungen
#include "afxcview.h"


#include "Shlwapi.h"

#include <map>
#include <vector>
#include <list>

#include <assert.h>
#include <iostream>

#include "misc\saprefsdialog.h"
#include "misc\saprefssubdlg.h"

#include "../AsyncSocketEx.h"
#include <afxdhtml.h>

#define CStdString CString
#define CStdStringW CStringW
#define CStdStringA CStringA

#include "../conversion.h"

#define USERCONTROL_GETLIST 0
#define USERCONTROL_CONNOP 1
#define USERCONTROL_KICK 2
#define USERCONTROL_BAN 3

#define USERCONTROL_CONNOP_ADD 0
#define USERCONTROL_CONNOP_CHANGEUSER 1
#define USERCONTROL_CONNOP_REMOVE 2
#define USERCONTROL_CONNOP_TRANSFERINFO 3
#define USERCONTROL_CONNOP_TRANSFEROFFSETS 4

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ fügt unmittelbar vor der vorhergehenden Zeile zusätzliche Deklarationen ein.

#endif // !defined(AFX_STDAFX_H__0D7D6CEC_E1AA_4287_BB10_A97FA4D444B6__INCLUDED_)
