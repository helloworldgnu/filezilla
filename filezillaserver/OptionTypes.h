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

#ifndef FZS_OPTION_TYPES_INCLUDED
#define FZS_OPTION_TYPES_INCLUDED

#define OPTION_SERVERPORT 1
#define OPTION_THREADNUM 2
#define OPTION_MAXUSERS 3
#define OPTION_TIMEOUT 4
#define OPTION_NOTRANSFERTIMEOUT 5
#define OPTION_CHECK_DATA_CONNECTION_IP 6
#define OPTION_SERVICE_NAME 7
#define OPTION_SERVICE_DISPLAY_NAME 8
#define OPTION_TLS_REQUIRE_SESSION_RESUMPTION 9
#define OPTION_LOGINTIMEOUT 10
#define OPTION_LOGSHOWPASS 11
#define OPTION_CUSTOMPASVIPTYPE 12
#define OPTION_CUSTOMPASVIP 13
#define OPTION_CUSTOMPASVMINPORT 14
#define OPTION_CUSTOMPASVMAXPORT 15
#define OPTION_WELCOMEMESSAGE 16
#define OPTION_ADMINPORT 17
#define OPTION_ADMINPASS 18
#define OPTION_ADMINIPBINDINGS 19
#define OPTION_ADMINIPADDRESSES 20
#define OPTION_ENABLELOGGING 21
#define OPTION_LOGLIMITSIZE 22
#define OPTION_LOGTYPE 23
#define OPTION_LOGDELETETIME 24
#define OPTION_DISABLE_IPV6 25
#define OPTION_ENABLE_HASH 26
#define OPTION_DOWNLOADSPEEDLIMITTYPE 27
#define OPTION_UPLOADSPEEDLIMITTYPE 28
#define OPTION_DOWNLOADSPEEDLIMIT 29
#define OPTION_UPLOADSPEEDLIMIT 30
#define OPTION_BUFFERSIZE 31
#define OPTION_CUSTOMPASVIPSERVER 32
#define OPTION_USECUSTOMPASVPORT 33
#define OPTION_MODEZ_USE 34
#define OPTION_MODEZ_LEVELMIN 35
#define OPTION_MODEZ_LEVELMAX 36
#define OPTION_MODEZ_ALLOWLOCAL 37
#define OPTION_MODEZ_DISALLOWED_IPS 38
#define OPTION_IPBINDINGS 39
#define OPTION_IPFILTER_ALLOWED 40
#define OPTION_IPFILTER_DISALLOWED 41
#define OPTION_WELCOMEMESSAGE_HIDE 42
#define OPTION_ENABLETLS 43
#define OPTION_ALLOWEXPLICITTLS 44
#define OPTION_TLSKEYFILE 45
#define OPTION_TLSCERTFILE 46
#define OPTION_TLSPORTS 47
#define OPTION_TLSFORCEEXPLICIT 48
#define OPTION_BUFFERSIZE2 49
#define OPTION_FORCEPROTP 50
#define OPTION_TLSKEYPASS 51
#define OPTION_SHAREDWRITE 52
#define OPTION_NOEXTERNALIPONLOCAL 53
#define OPTION_ACTIVE_IGNORELOCAL 54
#define OPTION_AUTOBAN_ENABLE 55
#define OPTION_AUTOBAN_ATTEMPTS 56
#define OPTION_AUTOBAN_TYPE 57
#define OPTION_AUTOBAN_BANTIME 58
#define OPTION_TLS_MINVERSION 59

#define OPTIONS_NUM 59

#define CONST_WELCOMEMESSAGE_LINESIZE 75

struct t_Option
{
	TCHAR name[30];
	int nType;
	BOOL bOnlyLocal; //If TRUE, setting can only be changed from local connections
};

const DWORD SERVER_VERSION = 0x00095900;
const DWORD PROTOCOL_VERSION = 0x00014000;

//												Name					Type		Not remotely
//																(0=str, 1=numeric)   changeable
static const t_Option m_Options[OPTIONS_NUM]={	_T("Serverports"),				0,	FALSE,
												_T("Number of Threads"),		1,	FALSE,
												_T("Maximum user count"),		1,	FALSE,
												_T("Timeout"),					1,	FALSE,
												_T("No Transfer Timeout"),		1,	FALSE,
												_T("Check data connection IP"),	1,	FALSE,
												_T("Service name"),				0,	TRUE,
												_T("Service display name"),		0,	TRUE,
												_T("Force TLS session resumption"), 1, FALSE,
												_T("Login Timeout"),			1,	FALSE,
												_T("Show Pass in Log"),			1,	FALSE,
												_T("Custom PASV IP type"),		1,	FALSE,
												_T("Custom PASV IP"),			0,	FALSE,
												_T("Custom PASV min port"),		1,	FALSE,
												_T("Custom PASV max port"),		1,	FALSE,
												_T("Initial Welcome Message"),	0,	FALSE,
												_T("Admin port"),				1,	TRUE,
												_T("Admin Password"),			0,	TRUE,
												_T("Admin IP Bindings"),		0,	TRUE,
												_T("Admin IP Addresses"),		0,	TRUE,
												_T("Enable logging"),			1,	FALSE,
												_T("Logsize limit"),			1,	FALSE,
												_T("Logfile type"),				1,	FALSE,
												_T("Logfile delete time"),		1,	FALSE,
												_T("Disable IPv6"),				1,  FALSE,
												_T("Enable HASH"),				1,  FALSE,
												_T("Download Speedlimit Type"),	1,	FALSE,
												_T("Upload Speedlimit Type"),	1,	FALSE,
												_T("Download Speedlimit"),		1,	FALSE,
												_T("Upload Speedlimit"),		1,	FALSE,
												_T("Buffer Size"),				1,	FALSE,
												_T("Custom PASV IP server"),	0,	FALSE,
												_T("Use custom PASV ports"),	1,	FALSE,
												_T("Mode Z Use"),				1,	FALSE,
												_T("Mode Z min level"),			1,	FALSE,
												_T("Mode Z max level"),			1,	FALSE,
												_T("Mode Z allow local"),		1,	FALSE,
												_T("Mode Z disallowed IPs"),	0,	FALSE,
												_T("IP Bindings"),				0,	FALSE,
												_T("IP Filter Allowed"),		0,	FALSE,
												_T("IP Filter Disallowed"),		0,	FALSE,
												_T("Hide Welcome Message"),		1,	FALSE,
												_T("Enable SSL"),				1,	FALSE,
												_T("Allow explicit SSL"),		1,	FALSE,
												_T("SSL Key file"),				0,	FALSE,
												_T("SSL Certificate file"),		0,	FALSE,
												_T("Implicit SSL ports"),		0,	FALSE,
												_T("Force explicit SSL"),		1,	FALSE,
												_T("Network Buffer Size"),		1,	FALSE,
												_T("Force PROT P"),				1,	FALSE,
												_T("SSL Key Password"),			0,	FALSE,
												_T("Allow shared write"),		1,	FALSE,
												_T("No External IP On Local"),	1,	FALSE,
												_T("Active ignore local"),		1,	FALSE,
												_T("Autoban enable"),			1,	FALSE,
												_T("Autoban attempts"),			1,	FALSE,
												_T("Autoban type"),				1,	FALSE,
												_T("Autoban time"),				1,	FALSE,
												_T("Minimum TLS version"),		1,	FALSE
											};

#endif
