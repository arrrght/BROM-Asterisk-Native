#include "stdafx.h"

#ifdef __linux__
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#endif

#include <wchar.h>
#include <stdio.h>
#include <string>
#include <array>
#include <vector>
#include <regex>

#include "AddInNative.h"

#pragma comment(lib, "Ws2_32.lib")

static const wchar_t *g_PropNames[] = {
	L"Connected",
	L"Listen",
	L"RegEx",
	L"Version",
	L"ErrorAsEvent",
};

static const wchar_t *g_PropNamesRu[] = {
	L"����������",
	L"������������������",
	L"�������������������",
	L"������",
	L"����������������",
	L"�������������",
	L"������������"
};


static const wchar_t *g_MethodNames[] = {
	L"Connect",
	L"Disconnect",
	L"SendCommand",
	L"ListenMode",
	L"SetRegEx"
};

static const wchar_t *g_MethodNamesRu[] = {
	L"������������",
	L"�����������",
	L"����������������",
	L"������������������",
	L"�����������������������������"
};


static const wchar_t g_kClassNames[] = L"CAddInNative";

static IAddInDefBase *pAsyncEvent = NULL;


uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len = 0);
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len = 0);
uint32_t getLenShortWcharStr(const WCHAR_T* Source);

static AppCapabilities g_capabilities = eAppCapabilitiesInvalid;

//---------------------------------------------------------------------------//
AppCapabilities SetPlatformCapabilities(const AppCapabilities capabilities)
{
	g_capabilities = capabilities;
	return eAppCapabilitiesLast;
}

//---------------------------------------------------------------------------//
long GetClassObject(const wchar_t* wsName, IComponentBase** pInterface)
{
	if (!*pInterface)
	{
		*pInterface = new CAddInNative();
		return (long)*pInterface;
	}
	return 0;
}
//---------------------------------------------------------------------------//
long DestroyObject(IComponentBase** pIntf)
{
	
	if (!*pIntf)
		return -1;

	delete *pIntf;
	*pIntf = 0;
	return 0;
}
//---------------------------------------------------------------------------//
const WCHAR_T* GetClassNames()
{
	static WCHAR_T* names = 0;
	if (!names)
		::convToShortWchar(&names, g_kClassNames);
	return names;
}


// ����� ��������� �������� // ��� ��������������� �����
static unsigned int _stdcall RecvInThread(void*p)
{
	
	CAddInNative *tcpCl = (CAddInNative*)p;

	int RCVBUFSIZE = 65536;
	
	char *buf = new char[RCVBUFSIZE];
	//char buf[RCVBUFSIZE] = {0};
	int recived = 0;
	bool disconnect = false;

	std::regex demo_r("[\\s\\-]\\d{5,5}");
	std::string demo_repl("*******");

	std::string s1 = "";

	std::size_t cutAt;

	const std::string SEPLN("\r\n\r\n");

	while (tcpCl->connected && tcpCl->listen)
	{
		Sleep(50);
		recived = recv(tcpCl->ConnectSocket, buf, RCVBUFSIZE-1, 0);
		if (recived > 0)
		{
			
			s1.append(buf, recived);
							
			while ((cutAt = s1.find(SEPLN)) != s1.npos) {
				if (cutAt > 0) 
				{

					wchar_t* res = 0;

					std::string nstr(s1.substr(0, cutAt + SEPLN.length()));
					s1 = s1.substr(cutAt + SEPLN.length());
					res = CHAR_2_WCHAR((char*)nstr.c_str());
					tcpCl->SendEvent(L"Received", res);
				}

			}

		}
		else
		{
			int nError = WSAGetLastError();
			if (nError != WSAEWOULDBLOCK && nError != 0)
			{
				disconnect = true;
				break;
			}
		}
	}

	if (tcpCl->hTh)
		CloseHandle(tcpCl->hTh);

	tcpCl->hTh = 0;

	if (disconnect && tcpCl->connected)
		tcpCl->OnDisconnect();


	return 0;
}


//---------------------------------------------------------------------------//
//CAddInNative
CAddInNative::CAddInNative()
{
	m_iMemory = 0;
	m_iConnect = 0;
}
//---------------------------------------------------------------------------//
CAddInNative::~CAddInNative()
{
}
//---------------------------------------------------------------------------//
bool CAddInNative::Init(void* pConnection)
{
	m_iConnect = (IAddInDefBase*)pConnection;

	if (m_iConnect == NULL)	return false;

	if (m_iConnect)
		m_iConnect->SetEventBufferDepth(100);

	connected = false;
	listen = false;
	ConnectSocket = NULL;
	hTh = NULL;
	errorAsEvent = false;
	
	// Initialise Winsock
	WSADATA WsaDat;
	if (WSAStartup(MAKEWORD(2, 2), &WsaDat) != 0)
	{
		//AddError
		WSACleanup();

		return false;
	}
	
	ULONG len = 100;

	connected = false;
		
	return true;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetInfo()
{
	return 2000;
}
//---------------------------------------------------------------------------//
void CAddInNative::Done()
{
	Disconnect();
}
//---------------------------------------------------------------------------//
bool CAddInNative::RegisterExtensionAs(WCHAR_T** wsExtensionName)
{
	const wchar_t* wsExtension = L"BROM-Asterisk-Native";
	int iActualSize = ::wcslen(wsExtension) + 1;
	WCHAR_T* dest = 0;

	if (m_iMemory)
	{
		if (m_iMemory->AllocMemory((void**)wsExtensionName, iActualSize * sizeof(WCHAR_T)))
			::convToShortWchar(wsExtensionName, wsExtension, iActualSize);
		return true;
	}

	return false;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNProps()
{
	return ePropLast;
}
//---------------------------------------------------------------------------//
long CAddInNative::FindProp(const WCHAR_T* wsPropName)
{
	long plPropNum = -1;
	wchar_t* propName = 0;

	::convFromShortWchar(&propName, wsPropName);
	plPropNum = findName(g_PropNames, propName, ePropLast);

	if (plPropNum == -1)
		plPropNum = findName(g_PropNamesRu, propName, ePropLast);

	delete[] propName;

	return plPropNum;
}
//---------------------------------------------------------------------------//
const WCHAR_T* CAddInNative::GetPropName(long lPropNum, long lPropAlias)
{
	if (lPropNum >= ePropLast)
		return NULL;

	wchar_t *wsCurrentName = NULL;
	WCHAR_T *wsPropName = NULL;
	int iActualSize = 0;

	switch (lPropAlias)
	{
	case 0: // First language
		wsCurrentName = (wchar_t*)g_PropNames[lPropNum];
		break;
	case 1: // Second language
		wsCurrentName = (wchar_t*)g_PropNamesRu[lPropNum];
		break;
	default:
		return 0;
	}

	iActualSize = wcslen(wsCurrentName) + 1;

	if (m_iMemory && wsCurrentName)
	{
		if (m_iMemory->AllocMemory((void**)&wsPropName, iActualSize * sizeof(WCHAR_T)))
			::convToShortWchar(&wsPropName, wsCurrentName, iActualSize);
	}

	return wsPropName;
}

//---------------------------------------------------------------------------//
bool CAddInNative::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
	wchar_t* str_var;
	size_t size_str_var = 0;

	switch (lPropNum)
	{

	case ePropConnected:
		{
		TV_VT(pvarPropVal) = VTYPE_BOOL;
		TV_I4(pvarPropVal) = connected;
		break;
		}
	case ePropListen:
		{
		TV_VT(pvarPropVal) = VTYPE_BOOL;
		TV_I4(pvarPropVal) = listen;
		break;
		}
	case ePropRegEx:
		{
		str_var = regEx;
		size_str_var = wcslen(str_var);

		if (m_iMemory)
		{
			if (m_iMemory->AllocMemory((void**)&pvarPropVal->pwstrVal, size_str_var * sizeof(WCHAR_T)))
			{
				::convToShortWchar(&pvarPropVal->pwstrVal, str_var, size_str_var);
				pvarPropVal->strLen = size_str_var;
				TV_VT(pvarPropVal) = VTYPE_PWSTR;
			}
		}
		break;
		}

	case ePropVersion:
		{
		str_var = L"1.0.1.11";
		size_str_var = wcslen(str_var);

		if (m_iMemory)
		{
			if (m_iMemory->AllocMemory((void**)&pvarPropVal->pwstrVal, size_str_var * sizeof(WCHAR_T)))
			{
				::convToShortWchar(&pvarPropVal->pwstrVal, str_var, size_str_var);
				pvarPropVal->strLen = size_str_var;
				TV_VT(pvarPropVal) = VTYPE_PWSTR;
			}
		}
		break;
		}
	case ePropErrorAsEvent:
		{
		TV_VT(pvarPropVal) = VTYPE_BOOL;
		TV_I4(pvarPropVal) = errorAsEvent;
		break;
		}
	default:
		{
		return false;
		}
	}

	return true;
}
//---------------------------------------------------------------------------//
bool CAddInNative::SetPropVal(const long lPropNum, tVariant* varPropVal)
{
	switch (lPropNum) // �� ������, ��� ��������� �������� �� ������������, � ������ ��������
	{

	case ePropErrorAsEvent:
		{
		if (TV_VT(varPropVal) != VTYPE_BOOL)
			return false;
		errorAsEvent = TV_BOOL(varPropVal);
		return true;
		break;
		}
	default:
		{
		return false;
		}
	}

	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::IsPropReadable(const long lPropNum)
{
	return true; // ��� �������� ����� ������
}
//---------------------------------------------------------------------------//
bool CAddInNative::IsPropWritable(const long lPropNum)
{
	switch (lPropNum)
	{

	case ePropErrorAsEvent:
		return true;
	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNMethods()
{
	return eMethodLast;
}
//---------------------------------------------------------------------------//
long CAddInNative::FindMethod(const WCHAR_T* wsMethodName)
{
	long plMethodNum = -1;
	wchar_t* name = 0;

	::convFromShortWchar(&name, wsMethodName);

	plMethodNum = findName(g_MethodNames, name, eMethodLast);

	if (plMethodNum == -1)
		plMethodNum = findName(g_MethodNamesRu, name, eMethodLast);

	delete[] name;

	return plMethodNum;
}
//---------------------------------------------------------------------------//
const WCHAR_T* CAddInNative::GetMethodName(const long lMethodNum, const long lMethodAlias)
{
	if (lMethodNum >= eMethodLast)
		return NULL;

	wchar_t *wsCurrentName = NULL;
	WCHAR_T *wsMethodName = NULL;
	int iActualSize = 0;

	switch (lMethodAlias)
	{
	case 0: // First language
		wsCurrentName = (wchar_t*)g_MethodNames[lMethodNum];
		break;
	case 1: // Second language
		wsCurrentName = (wchar_t*)g_MethodNamesRu[lMethodNum];
		break;
	default:
		return 0;
	}

	iActualSize = wcslen(wsCurrentName) + 1;

	if (m_iMemory && wsCurrentName)
	{
		if (m_iMemory->AllocMemory((void**)&wsMethodName, iActualSize * sizeof(WCHAR_T)))
			::convToShortWchar(&wsMethodName, wsCurrentName, iActualSize);
	}

	return wsMethodName;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNParams(const long lMethodNum)
{
	switch (lMethodNum)
	{
	case eMethodSendCommand:
	case eMethodListenMode:
	case eMethodSetRegEx:
		return 1;
	case eMethodConnect:
		return 2;
	default:
		return 0;
	}

	return 0;
}
//---------------------------------------------------------------------------//
bool CAddInNative::GetParamDefValue(const long lMethodNum, const long lParamNum, tVariant *pvarParamDefValue)
{
	TV_VT(pvarParamDefValue) = VTYPE_EMPTY;

	switch (lMethodNum)
	{

	case eMethodConnect:
		switch (lParamNum)
		{
		case 1: // port 

			TV_VT(pvarParamDefValue) = VTYPE_PWSTR;
			TV_WSTR(pvarParamDefValue) = L"5038";
			return true;

		}

	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::HasRetVal(const long lMethodNum)
{
	switch (lMethodNum)
	{
	case eMethodConnect:
	case eMethodDisconnect:
	case eMethodSendCommand:
	case eMethodListenMode:
	case eMethodSetRegEx:
		return true;
	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::CallAsProc(const long lMethodNum, tVariant* paParams, const long lSizeArray)
{
	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
	switch (lMethodNum)
	{
	case eMethodConnect:
	{
		if (!lSizeArray || !paParams)
			return false; // ���� ��� ���������� �� ������

		if (TV_VT(paParams) != VTYPE_PWSTR) // ��������� ��� ������� ��������� ������
			return false;

		wchar_t* server = 0;
		::convFromShortWchar(&server, TV_WSTR(paParams));

		if (TV_VT(paParams + 1) != VTYPE_PWSTR) // ��������� ��� ������� ��������� ����
			return false;

		wchar_t* port = 0;
		::convFromShortWchar(&port, TV_WSTR(paParams + 1));

		TV_VT(pvarRetValue) = VTYPE_BOOL;
		TV_BOOL(pvarRetValue) = Connect(server, port);

		return true;

	}
	case eMethodDisconnect:
		{
			if (lSizeArray || paParams)
				return false; // ���� ���� ��������� �� ������

			TV_VT(pvarRetValue) = VTYPE_BOOL;
			TV_BOOL(pvarRetValue) = Disconnect();

			return true;

		}

	case eMethodSendCommand:
		{
			if (!lSizeArray || !paParams)
				return false; // ���� ��� ���������� �� ������

			if (TV_VT(paParams) != VTYPE_PWSTR) // ��������� ��� ������� ���������
				return false;

			wchar_t* msg = 0;
			::convFromShortWchar(&msg, TV_WSTR(paParams));

			TV_VT(pvarRetValue) = VTYPE_BOOL;
			TV_BOOL(pvarRetValue) = SendCommand(msg);

			return true;
		}

	case eMethodListenMode:
		{
			if (!lSizeArray || !paParams)
				return false; // ���� ��� ���������� �� ������

			if (TV_VT(paParams) != VTYPE_BOOL) // ��������� ��� ������� ���������
				return false;


			TV_VT(pvarRetValue) = VTYPE_BOOL;
			TV_BOOL(pvarRetValue) = ListenMode(TV_BOOL(paParams));

			return true;

		}

	case eMethodSetRegEx:
		{
			if (!lSizeArray || !paParams)
				return false; // ���� ��� ���������� �� ������

			if (TV_VT(paParams) != VTYPE_PWSTR) // ��������� ��� ������� ���������
				return false;
			try
			{
				wchar_t* regEx = 0;
				::convFromShortWchar(&regEx, TV_WSTR(paParams));

				std::wregex r(regEx);

				TV_VT(pvarRetValue) = VTYPE_BOOL;
				TV_BOOL(pvarRetValue) = setRegEx(regEx);

				return true;
			}
			catch (const std::exception&)
			{
				TV_VT(pvarRetValue) = VTYPE_BOOL;
				TV_BOOL(pvarRetValue) = false;
				return false;
			}
		}

	default:
		return false;
	}
	return false;
}

//---------------------------------------------------------------------------//
void CAddInNative::SetLocale(const WCHAR_T* loc)
{
#ifndef __linux__
	_wsetlocale(LC_ALL, loc);
#else
	int size = 0;
	char *mbstr = 0;
	wchar_t *tmpLoc = 0;
	convFromShortWchar(&tmpLoc, loc);
	size = wcstombs(0, tmpLoc, 0) + 1;
	mbstr = new char[size];

	if (!mbstr)
	{
		delete[] tmpLoc;
		return;
	}

	memset(mbstr, 0, size);
	size = wcstombs(mbstr, tmpLoc, wcslen(tmpLoc));
	setlocale(LC_ALL, mbstr);
	delete[] tmpLoc;
	delete[] mbstr;
#endif
}
//---------------------------------------------------------------------------//
bool CAddInNative::setMemManager(void* mem)
{
	m_iMemory = (IMemoryManager*)mem;
	return m_iMemory != 0;
}

//---------------------------------------------------------------------------//
long CAddInNative::findName(const wchar_t* names[], const wchar_t* name,
	const uint32_t size) const
{
	long ret = -1;
	for (uint32_t i = 0; i < size; i++)
	{
		if (!wcscmp(names[i], name))
		{
			ret = i;
			break;
		}
	}
	return ret;
}

//---------------------------------------------------------------------------//
uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len)
{
	if (!len)
		len = ::wcslen(Source) + 1;

	if (!*Dest)
		*Dest = new WCHAR_T[len];

	WCHAR_T* tmpShort = *Dest;
	wchar_t* tmpWChar = (wchar_t*)Source;
	uint32_t res = 0;

	::memset(*Dest, 0, len * sizeof(WCHAR_T));
	do
	{
		*tmpShort++ = (WCHAR_T)*tmpWChar++;
		++res;
	} while (len-- && *tmpWChar);

	return res;
}
//---------------------------------------------------------------------------//
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len)
{
	if (!len)
		len = getLenShortWcharStr(Source) + 1;

	if (!*Dest)
		*Dest = new wchar_t[len];

	wchar_t* tmpWChar = *Dest;
	WCHAR_T* tmpShort = (WCHAR_T*)Source;
	uint32_t res = 0;

	::memset(*Dest, 0, len * sizeof(wchar_t));
	do
	{
		*tmpWChar++ = (wchar_t)*tmpShort++;
		++res;
	} while (len-- && *tmpShort);

	return res;
}
//---------------------------------------------------------------------------//
uint32_t getLenShortWcharStr(const WCHAR_T* Source)
{
	uint32_t res = 0;
	WCHAR_T *tmpShort = (WCHAR_T*)Source;

	while (*tmpShort++)
		++res;

	return res;
}
//---------------------------------------------------------------------------//

void CAddInNative::addError(uint32_t wcode, const wchar_t* source, const wchar_t* descriptor, long code)
{
	if (m_iConnect)
	{
		WCHAR_T *err = 0;
		WCHAR_T *descr = 0;

		::convToShortWchar(&err, source);
		::convToShortWchar(&descr, descriptor);

		m_iConnect->AddError(wcode, err, descr, code);
		delete[] err;
		delete[] descr;
	}
}

//---------------------------------------------------------------------------//
bool CAddInNative::SendEvent(wchar_t* msg, wchar_t* Data)
{
	bool res;
	if (m_iConnect)
	{
		if (regEx != L"")
		{
			try
			{
				std::wregex r(regEx);

				if (std::regex_search(Data, r))
				{
					res = m_iConnect->ExternalEvent(wsName, msg, Data);
					return res;
				}

			}
			catch (const std::regex_error& e)
			{
				switch (e.code())
				{
				case std::regex_constants::error_collate:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_collate");
					return res;
				case std::regex_constants::error_ctype:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_ctype");
					return res;
				case std::regex_constants::error_escape:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_escape");
					return res;
				case std::regex_constants::error_backref:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_backref");
					return res;
				case std::regex_constants::error_brack:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_brack");
					return res;
				case std::regex_constants::error_paren:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_paren");
					return res;
				case std::regex_constants::error_brace:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_brace");
					return res;
				case std::regex_constants::error_badbrace:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_badbrace");
					return res;
				case std::regex_constants::error_range:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_range");
					return res;
				case std::regex_constants::error_space:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_space");
					return res;
				case std::regex_constants::error_badrepeat:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_badrepeat");
					return res;
				case std::regex_constants::error_complexity:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_complexity");
					return res;
				case std::regex_constants::error_stack:
					res = m_iConnect->ExternalEvent(wsName, L"������������ ������ Regex", L"error_stack");
					return res;

				default:
					break;
				}
			}
			
			
		}
		else
		{
			res = m_iConnect->ExternalEvent(wsName, msg, Data);
			return res;
		}
	}

	return false;
}


// ���������� WINSOCK ����������� ���������� 0 (����) ���� ���������� ������� �������
bool CAddInNative::Connect(wchar_t* server, wchar_t* port)
{
	WSADATA wsaData;

	if (connected)	Disconnect();

	connected = false;

	int iResult;
	INT iRetval;

	DWORD dwRetval;

	int i = 1;

	ADDRINFOW *result = NULL;
	ADDRINFOW *ptr = NULL;
	ADDRINFOW hints;

	LPSOCKADDR sockaddr_ip;

	wchar_t ipstringbuffer[46];
	DWORD ipbufferlength = 46;


	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		return false;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	dwRetval = GetAddrInfoW(server, port, &hints, &result);
	if (dwRetval != 0) {
		WSACleanup();
		return false;
	}

	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		if (ptr->ai_family == AF_UNSPEC) return false;

		sockaddr_ip = (LPSOCKADDR)ptr->ai_addr;
		ipbufferlength = 46;
		iRetval = WSAAddressToString(sockaddr_ip, (DWORD)ptr->ai_addrlen, NULL, ipstringbuffer, &ipbufferlength);

	}

	ConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	iResult = connect(ConnectSocket, result->ai_addr, result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		iResult = closesocket(ConnectSocket);
		WSACleanup();
		return false;
	}

	if (ConnectSocket == INVALID_SOCKET) {
		WSACleanup();
		return false;
	}

	FreeAddrInfoW(result);
	
	connected = true;
	int lensrvport = wcslen(server) + wcslen(port) + 1;
	
	wchar_t server_port[1000] = L"Server:";
	
	wcsncat_s(server_port, server, 200);
	wcsncat_s(server_port, L" Port:", 7);
	wcsncat_s(server_port, port, 6);
	
	SendEvent(L"Connected", server_port);

	return true; // ��� ��
}

bool CAddInNative::Disconnect()
{
	if (!connected)
		return true;

	connected = false;


	wchar_t str[256];

	int res = 0;
	bool bRes = true;

	try {
		res = shutdown(ConnectSocket, 2);
		str[0] = 0;

		linger l;
		l.l_linger = 1;
		l.l_onoff = 1;

		res = setsockopt(ConnectSocket, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l));
		res = closesocket(ConnectSocket);

		if (res == SOCKET_ERROR)
		{
			OnError();
			bRes = false;
			//SendErrorEvent(str);
		}
	}
	catch (...) {
		bRes = false;
	};

	if (hTh)
	{
		WaitForSingleObject(hTh, INFINITE);
		hTh = 0;
	}


	return bRes;
}

bool CAddInNative::ListenMode(int flag)
{
	if (flag == 1)
	{
		

		hTh = 0;

		unsigned int thID;

		listen = 1;
		hTh = (HANDLE)_beginthreadex(NULL, 10, RecvInThread, (LPVOID)this, 0, &thID);

		if (hTh == 0)
			return 0;
	}
	else
	{

		if (hTh)
		{
			listen = 0;
			DWORD stopThRes = WaitForSingleObject(hTh, INFINITE);
			hTh = 0;
			if (stopThRes != WAIT_OBJECT_0)
				return 0;
		}

	}

	return 1;
}

bool CAddInNative::SendCommand(wchar_t* msg)
{

	int sendCount = 0;

	if (!connected)
	{
		//OnError(L"Send",0,L"No connection");
		return false;
	}
	else
	{

		char* toSend = WCHAR_2_CHAR(msg);
		int l = strlen(toSend);
		sendCount = send(ConnectSocket, toSend, l, 0);
		if (sendCount == SOCKET_ERROR)
		{

			OnError();
			OnDisconnect();
			return false;
		}

		return true;
	}

}


bool CAddInNative::setRegEx(wchar_t* str_regex)
{
		regEx = str_regex;
		if (wcslen(str_regex) == 0)
			regEx = L"";

		return true;
}


// ����������� ������-�������

void CAddInNative::OnDisconnect()
{
	if (connected)
	{
		SendEvent(L"Disconnected", L"Disconnected");
		Disconnect();
		connected = false;
	}
}

void CAddInNative::OnError()
{
	DWORD dwErr = GetLastError();
	if (dwErr)
		OnError(dwErr, getErrorDescription(dwErr));
}

void CAddInNative::OnError(long scode, wchar_t *descr)
{
	//if (ErrorAsEvent)


	if (m_iConnect)
		m_iConnect->AddError(ADDIN_E_FAIL, wsName, descr, scode); //���� scode ����� �� ������� �������� � ����� ������������� ����������, ������� ����� ���� ����������� � ���������� ���������� ����������� ����� 1�:�����������.


}

wchar_t* CAddInNative::getErrorDescription(DWORD dwErr)
{
	LPVOID lpMsgBuf;
	::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	LPWSTR str = (LPWSTR)lpMsgBuf;
	return str;
}

// --------------------------------------------------------------Innova-IT ------------------------------------------------------------------------//
bool CAddInNative::GetValueFromKey(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValue, LPVOID pBuffer, ULONG uSize)
{
	HKEY hTargetKey;
	LONG lRet;
	//ULONG uSz = uSize;

	lRet = RegOpenKeyEx(hKey, lpSubKey, 0, KEY_READ | KEY_WOW64_64KEY, &hTargetKey);

	if (lRet != ERROR_SUCCESS) return FALSE;

	lRet = RegQueryValueEx(hTargetKey, lpValue, 0, 0, (LPBYTE)pBuffer, &uSize);

	if (lRet != ERROR_SUCCESS)
	{
		RegCloseKey(hTargetKey);
		return FALSE;
	}

	RegCloseKey(hTargetKey);
	return TRUE;
}


// --------------------------------------------------------------Innova-IT ------------------------------------------------------------------------//
char* WCHAR_2_CHAR(wchar_t *in_str)
{
	int len = wcslen(in_str) / sizeof(char) + 1;
	char* out_str = new char[len];
	size_t* out_str_len = 0;
	wcstombs_s(out_str_len, out_str, len, in_str, _TRUNCATE);

	return out_str;
}

// --------------------------------------------------------------Innova-IT ------------------------------------------------------------------------//
wchar_t* CHAR_2_WCHAR(char *in_str)
{
	int len = strlen(in_str) + 1;
	wchar_t* out_str = new wchar_t[len * sizeof(wchar_t)];

	size_t* out_str_len = 0;

	mbstowcs_s(out_str_len, out_str, len, in_str, _TRUNCATE);

	return out_str;
}

// --------------------------------------------------------------Innova-IT ------------------------------------------------------------------------//
std::vector<std::string> resplit(const std::string & s, std::string rgx_str)
{


	std::vector<std::string> elems;

	std::regex rgx(rgx_str);

	std::sregex_token_iterator iter(s.begin(), s.end(), rgx, -1);
	std::sregex_token_iterator end;

	while (iter != end) {
		elems.push_back(*iter);
		++iter;
	}

	return elems;

}
