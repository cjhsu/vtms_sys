//---------------------------------------------------------------------------
//  util.cpp:
//---------------------------------------------------------------------------
//
//  Purpose:
//
//      shared utility functions
//
//  Author:
//     Written by Charles Hsu (cjhsu@ms2.hinet.net)
//     Copyright © 2005-2010 Friendly Information. All Rights Reserved.
//     http://www.fi.com.tw
//
//  History:
//
//  Notes:
//
//---------------------------------------------------------------------------
#include <vcl>
#include <time.h>
#include <winsock2.h>
#include <memory>
#include <vector>
#include <tlhelp32.h>
#include <string>
#include <fstream>
#pragma hdrstop

#include "Tokenize.h"
#include "util.h"
#include "utf8.h"
//---------------------------------------------------------------------------

#pragma package(smart_init)

//---------------------------------------------------------------------------
char* __fastcall ticks_string( void )
{
	unsigned long ticks = ::GetTickCount();
	static char buf[sizeof(long)*8+1];
	ltoa( ticks, buf, 10 );

    return buf;
}
//------------------------------------------------------------------------------
char* __fastcall TimeStamp( SYSTEMTIME time )
{
    static char timeStamp[14] = {0};

    wsprintf( timeStamp, "%02d:%02d:%02d.%03d\0",
              time.wHour, time.wMinute, time.wSecond, time.wMilliseconds );
    return timeStamp;
}
//------------------------------------------------------------------------------
char *GetPeerSocketIp( SOCKET s )
{
    struct sockaddr_in peer;
    ZeroMemory( &peer, sizeof( peer ) );

    int size = sizeof( peer );
    if ( getpeername( s, (SOCKADDR *)&peer, &size ) == SOCKET_ERROR )
    {
        return NULL;
    }
    return inet_ntoa( peer.sin_addr );
}
//------------------------------------------------------------------------------
int __fastcall GetPeerSocketPort( SOCKET s )
{
    struct sockaddr_in peer;
    ZeroMemory( &peer, sizeof( peer ) );

    int size = sizeof( peer );
    if ( getpeername( s, (SOCKADDR *)&peer, &size ) == SOCKET_ERROR )
    {
        return 0; //ERROR
    }
    return (unsigned int)ntohs( peer.sin_port );
}
//------------------------------------------------------------------------------
String __fastcall utf8_to_ansi( const char *buf )
{
    String result = EmptyStr;
    int length = ::MultiByteToWideChar(CP_UTF8, 0, buf, -1, 0, 0);
    std::auto_ptr<wchar_t> widechar( new wchar_t[length] );
    if ( ::MultiByteToWideChar( CP_UTF8, 0, buf, -1, widechar.get(), length ) )
    {
        length = ::WideCharToMultiByte(CP_ACP, 0, widechar.get(), -1, 0, 0, 0, 0);
        std::auto_ptr<char> multichar( new char[length+1] );
        ZeroMemory( multichar.get(), length+1 );
        //CP_ACP = default to ANSI code page
        //CP_OEM = default to OEM  code page
        if ( ::WideCharToMultiByte(CP_ACP, 0, widechar.get(), -1, multichar.get(), length, 0, 0) )
        {
            result = multichar.get();
        }
        else
            return EmptyStr;
    }
    return result;
}

//------------------------------------------------------------------------------
String __fastcall ansi_to_utf8( const char *buf )
{
    int len = ::MultiByteToWideChar(CP_ACP, 0, buf, -1, NULL, 0);
    if (len == 0) return L"";  
  
    std::vector<wchar_t> result(len);
    ::MultiByteToWideChar(CP_ACP, 0, buf, -1, &result[0], len);

    return &result[0];
}
//---------------------------------------------------------------------------
void __fastcall Shuffle( char *nums, int a, int b )
{
    int i,j,k,temp;

    randomize();

    //for ( int i=0; i<(b-a)-1; i++ )
    //    nums[i] = i;
    for ( int i=0; i<(b-a)-1; i++ )
    {
        j = random(b-a+1);
        k = random(b-a+1);
        if ( j == k )
            continue;
        temp = nums[j];
        nums[j] = nums[k];
        nums[k] = temp;
    }
    //
    for ( int i=0; i<(b-a)-1; i++ )
        nums[i] = nums[i]+a;
}
//------------------------------------------------------------------------------
String __fastcall LongToString( long n )
{
    char buf[sizeof(long)*8+1];
    ltoa( n, buf, 10 );

    return String( buf );
}
//---------------------------------------------------------------------------
void __fastcall EnableDebugPriv( void )
{
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tkp;

    OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken );
    LookupPrivilegeValue( NULL, SE_DEBUG_NAME, &luid );
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Luid = luid;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges( hToken, false, &tkp, sizeof(tkp), NULL, NULL );
    CloseHandle( hToken );
}
//---------------------------------------------------------------------------
unsigned long __fastcall GetProcessId( char *name )
{
    unsigned long pid = 0;
    EnableDebugPriv();

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, NULL );
    if ( Process32First(snapshot, &entry) == TRUE )
    {
        while ( Process32Next(snapshot, &entry) == TRUE )
        {
            if ( stricmp(entry.szExeFile, name ) == 0)
            {
                HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION + PROCESS_VM_READ + PROCESS_TERMINATE, FALSE, entry.th32ProcessID );
                pid = entry.th32ProcessID;
                CloseHandle( hProcess );
            }
        }
    }
    CloseHandle(snapshot);
    return pid;
}
//---------------------------------------------------------------------------
void __fastcall ExtractStringToList(TStringList *list, String fields)
{
	int len = 0;
    std::string s = fields.c_str();
	std::vector<std::string>vstr = Tokenize(s, ",");

	// ¤À§O³B²z¨C­Ó¥H "," µ²§Àªº¦r¦ê
	for (vector<std::string>::iterator iter = vstr.begin(); iter != vstr.end();
	++iter) {
		len = strlen((*iter).c_str());
		//
		if (len == 0)
			list->Add(EmptyStr);
		else
			list->Add(String((*iter).c_str(), len));
	}
}

void __fastcall ExtractStringToList1(TStringList *list, String fields)
{
	int len = 0;
    std::string s = fields.c_str();
	std::vector<std::string>vstr = Tokenize(s, "\r\n");

	// ¤À§O³B²z¨C­Ó¥H "," µ²§Àªº¦r¦ê
	for (vector<std::string>::iterator iter = vstr.begin(); iter != vstr.end();
	++iter) {
		len = strlen((*iter).c_str());
		//
		if (len == 0)
			list->Add(EmptyStr);
		else
			list->Add(String((*iter).c_str(), len));
	}
}
//------------------------------------------------------------------------------
void __fastcall GetVersionInfo(int &major, int &minor, int &tiny, int &build)
{
	String szFile = Application->ExeName;
	DWORD dwVerInfoSize = GetFileVersionInfoSize(szFile.c_str(),
		&dwVerInfoSize);
	unsigned char *bVerInfoBuf = new unsigned char[dwVerInfoSize];
	if (GetFileVersionInfo(szFile.c_str(), 0, dwVerInfoSize, bVerInfoBuf)) {
		VS_FIXEDFILEINFO *vsInfo;
		UINT vsInfoSize;
		if (VerQueryValue(bVerInfoBuf, "\\", (void**)&vsInfo, &vsInfoSize)) {
			major = HIWORD(vsInfo->dwFileVersionMS);
			minor = LOWORD(vsInfo->dwFileVersionMS);
			tiny = HIWORD(vsInfo->dwFileVersionLS);
			build = LOWORD(vsInfo->dwFileVersionLS);
		}
	}
	delete[]bVerInfoBuf;
}
//------------------------------------------------------------------------------
bool __fastcall valid_utf8_file(const char* file_name)
{
    ifstream ifs(file_name);
    if (!ifs)
        return false; // even better, throw here

    istreambuf_iterator<char> it(ifs.rdbuf());
    istreambuf_iterator<char> eos;

    return utf8::is_valid(it, eos);
}
//------------------------------------------------------------------------------
bool __fastcall start_with_bom( void )
{
    unsigned char byte_order_mark[] = {0xef, 0xbb, 0xbf};
    bool result = utf8::starts_with_bom(byte_order_mark, byte_order_mark + sizeof(byte_order_mark));

    return result;
}
//------------------------------------------------------------------------------
//void __fastcall fix_utf8_string(std::string& str)
//{
//    std::string temp;
//    utf8::replace_invalid(str.begin(), str.end(), back_inserter(temp));
//    str = temp;
//}

char* __fastcall debug_info( unsigned long pid, unsigned long line )
{
    static char str[80] = {0};
    wsprintf( str, "pid:%s,line:%s",
        FormatFloat( "######", pid ).c_str(),
        FormatFloat( "#,###,##0", line ).c_str() );

    return str;
}

//------------------------------------------------------------------------------
// remove CR (0x0D) from string
String remove_cr( String in )
{
    ::string result = in.c_str();
    if ( *result.rbegin() == '\r' ) { result.resize(result.size() - 1); }
    return String(result.c_str());
}
//------------------------------------------------------------------------------
// remove LF (0x0A) from string
String remove_lf( String in )
{
    ::string result = in.c_str();
    if ( *result.rbegin() == '\n' ) { result.resize(result.size() - 1); }
    return String(result.c_str());
}
//------------------------------------------------------------------------------


