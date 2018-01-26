//---------------------------------------------------------------------------
//  Main.cpp:
//---------------------------------------------------------------------------
//
//  Purpose:
//
//  Author:
//     Written by Charles Hsu (cjhsu@ms2.hinet.net)
//     Copyright © 2005-2010 Friendly Information. All Rights Reserved.
//     http://www.fi.com.tw
//
//  History:
//      April 26 2006 - Initial creation.
//      May 28 2007 - Diebold reversion
//
//  Sample Usage:
//
//
//  Notes:
//
//---------------------------------------------------------------------------
#include <winsock2.h>
#include <stdio.h>
#include <vcl>
#include <vcl\inifiles.hpp>
#include <controls.hpp>
#include <vector>
#include <Clipbrd.hpp>
#include <dateutils.hpp>
#include <stdlib.h>
#include <StrUtils.hpp>

#pragma comment(lib,"ws2_32.lib")

//#define DEBUG
#include "json/json.h"
#include "util.h"
#include "GblDef.h"
#include "Main.h"
#include "Logger.h"
#include "TypedList.h"
#include "vtmsthread.h"
#include "DumpHex.h"
#include "utf8.h"
#include "qcv.h"

unsigned char dump_buff[DUMP_BUFF_SIZE*10];

HANDLE hOut;
SOCKET g_sdListen = INVALID_SOCKET;
HANDLE g_ThreadHandles[MAX_WORKER_THREAD] = {NULL};
CRITICAL_SECTION g_CriticalSection;// guard access to the global context list
HANDLE g_hIOCP = NULL;
bool g_bEndServer = false;
unsigned long lngIdleTimeoutMs = 0;
unsigned long lngElapsedTicks;
unsigned long lngSnapshotDelayMs = 0;
long lngImgSeq = 1;
bool check_length = true;
String listen_ip;
int listen_port;
TLogger *logger = NULL;

int intIndex = 0;
TTypedList <PER_SOCKET_CONTEXT> *contextList;
TStringList *dc_client;
TStringList *nvr;

Json::Reader reader;
Json::Value root;
Json::FastWriter fast_writer;

//vtms
bool vtms_enabled = false;
String http_host;
int http_port;

//AutoTest
bool autotest_enabled = false;
unsigned long lngAutoTestIntervalMs = 0;
unsigned long lngAutoTicks;
String test_nvr = EmptyStr;
int test_cam = 1;

typedef enum _VTMS_RESP_ERR
{
    VTMS_ERR_OK = 0,
    VTMS_ERR_PARAM = 5001,   // °Ñ¼Æ¿ù»~
    VTMS_ERR_MEMORY,
    VTMS_ERR_CH,
    VTMS_ERR_BUSY,
    VTMS_ERR_WRITE,
    VTMS_ERR_LOAD,
    VTMS_ERR_SQL,
    VTMS_ERR_EMPTY,
    VTMS_ERR_REG,
} VTMSErr;

//------------------------------------------------------------------------------
class TCastClass
{
    public:
        void __fastcall OnThreadTerminate(TObject *Sender)
        {
            TQCV *thread = dynamic_cast<TQCV*>(Sender);
            //printf( "\r\nthread %d : %s thread terminated\n",
            //    thread->ThreadID, thread->Name.c_str() );
            thread = (void*)NULL;
        }

        void __fastcall OnNvrResponse(TObject *Sender)
        {
            TQCV *thread = dynamic_cast<TQCV*>(Sender);
            //
            LOG_MSG( Format( "thread %d #NVR->%s",
                OPENARRAY(TVarRec,( (int)thread->ThreadID, thread->ResponseString ) ) ).c_str() );
            SetConsoleTextAttribute( hOut, BROWN );
            printf( "\r\nthread %d #NVR->%s\r\n", thread->ThreadID, thread->ResponseString.c_str() );
            SetConsoleTextAttribute( hOut, NORMAL );
            thread = (void*)NULL;
        }
} CastClass;
//------------------------------------------------------------------------------
void snapshot( String nvr_ip, int cam, String tag, String lines, String filename )
{
    static int seq = 1;

    TQCV *qcv8001 = new TQCV();
    //http_get->uri = "http://211.23.156.208:80/info";
    qcv8001->OnTerminate = TCastClass().OnThreadTerminate;
    qcv8001->OnResponse = TCastClass().OnNvrResponse;
    String text = Format( "http://%s:80/snapshot?tag=%s&ch=%d&lines=%s&filename=%s",
        OPENARRAY(TVarRec,( nvr_ip, tag, cam, lines, filename ) ) );
    qcv8001->URI = text;
    Clipboard()->AsText = text;
    SetConsoleTextAttribute( hOut, BROWN );
    printf( "\r\n#NVR<-%s\r\n", qcv8001->URI );
    SetConsoleTextAttribute( hOut, NORMAL );
    //
    LOG_MSG( Format( "#NVR<-%s", OPENARRAY(TVarRec,( qcv8001->URI ) ) ).c_str() );

    qcv8001->run = true;
}
//------------------------------------------------------------------------------
void test_snapshot( void )
{
    if ( test_nvr.IsEmpty() )
        test_nvr = "127.0.0.1";

    TDateTime dt = Now();
    String tag = FormatDateTime( "HHNNSSZZZ", dt );
    String filename = tag + ".png";
    String lines = FormatDateTime( "YYYY-MM-DD_HH:NN:SS", dt ) +
    "\r\n" + test_nvr + "_cam" + test_cam + "\r\n" + tag;
    //
    snapshot( test_nvr, test_cam, tag, lines, filename );
}
//------------------------------------------------------------------------------
void __fastcall LastErrorFormatMessage( String text )
{
    LPVOID lpMsgBuf = NULL;
    DWORD errCode = GetLastError();
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                   errCode,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPTSTR)&lpMsgBuf, 0, NULL );
    if ( errCode != 0 )
    {
        LOG_MSG( TrimRight( String((char*)lpMsgBuf) ).c_str() );
    }
    else
    {
        LOG_MSG( TrimRight( String((char*)lpMsgBuf) ).c_str() );
    }
    //LOG_MSG( text.c_str() );
    //
    //
    SetConsoleTextAttribute( hOut, RED );
    printf( "\nGetLastError=%u #%s\n", errCode, (char*)lpMsgBuf );
    SetConsoleTextAttribute( hOut, NORMAL );
    //
    LocalFree( lpMsgBuf );
}
//------------------------------------------------------------------------------
void __fastcall WSALastErrorFormatMessage( String text )
{
    LPVOID lpMsgBuf = NULL;

    int errCode = WSAGetLastError();
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                   errCode,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPTSTR)&lpMsgBuf, 0, NULL );

    LOG_MSG( TrimRight( String((char*)lpMsgBuf) ).c_str() );
    //LOG_MSG( text.c_str() );
    //
    SetConsoleTextAttribute( hOut, RED );
    printf( "\nWSAGetLastError=%u #%s\n", errCode, lpMsgBuf );
    SetConsoleTextAttribute( hOut, NORMAL );
    //
    LocalFree( lpMsgBuf );
}
//------------------------------------------------------------------------------
void __fastcall OnHttpStatus(TObject *Sender, String status)
{
    TThread *thread = dynamic_cast<TThread*>(Sender);
    LOG_MSG( Format( "thread %s #%s",
        OPENARRAY(TVarRec,( FormatFloat( "#####", thread->ThreadID ).c_str(), status.c_str() ) ) ).c_str() );
    //
    SetConsoleTextAttribute( hOut, DKBLUE );
    printf( "\nthread %d #%s\n", thread->ThreadID, status );
    SetConsoleTextAttribute( hOut, NORMAL );
}
//------------------------------------------------------------------------------
void __fastcall OnHttpException(TObject *Sender, String message)
{
    TThread *thread = dynamic_cast<TThread*>(Sender);
    LOG_MSG( Format( "thread %s #%s",
        OPENARRAY(TVarRec,( FormatFloat( "#####", thread->ThreadID ).c_str(),
        message.c_str() ) ) ).c_str() );
    //
    SetConsoleTextAttribute( hOut, RED );
    printf( "\nthread %d #%s\n", thread->ThreadID, message.c_str() );
    SetConsoleTextAttribute( hOut, NORMAL );
}
//------------------------------------------------------------------------------
void __fastcall OnVtmsHttpResponse(TObject *Sender, int code, String response)
{
    TVtmsThread *thread = dynamic_cast<TVtmsThread*>(Sender);
    LOG_MSG( Format( "thread %s #[%s] http response status code %d\n",
        OPENARRAY(TVarRec,( FormatFloat( "#####", thread->ThreadID ).c_str(),
        thread->Url.c_str(), code ) ) ).c_str() );
    LOG_PRINTF( Format( "%s\n", OPENARRAY(TVarRec,(thread->Url.c_str())) ).c_str() );
    LOG_PRINTF( Format( "%s\n", OPENARRAY(TVarRec,(response.c_str())) ).c_str() );
    //
    SetConsoleTextAttribute( hOut, DKBLUE );
    printf( "\nthread %d #http response %d\n", thread->ThreadID, code );
    printf( "%s\n", thread->Url );
    printf( "%s\n", response.c_str() );
    /*
    if ( reader.parse( response.c_str(), root ) ) //Jsoncpp ¥u¯àÅª ansi format
    {
        String status = ( utf8_to_ansi( (const char*)root["status"].asString().c_str() ) );
        String message = ( utf8_to_ansi( (const char*)root["message"].asString().c_str() ) );
        //String rowguid = ( utf8_to_ansi( (const char*)root["rowguid"].asString().c_str() ) );
        printf( "status : %s\nmessage : %s\n", status, message );
    }
    else
    {
        String errmsg = reader.getFormattedErrorMessages().c_str();
        LOG_MSG( Format( "thread %s : %s ",
            OPENARRAY(TVarRec,( FormatFloat( "#####", thread->ThreadID ).c_str(),
            errmsg.c_str() ) ) ).c_str() );
        puts( errmsg.c_str() );
    }
    */
    SetConsoleTextAttribute( hOut, NORMAL );
}
//------------------------------------------------------------------------------
void __fastcall BeforeVtmsHttpPost(TObject *Sender, String url, String body)
{
    TVtmsThread *thread = dynamic_cast<TVtmsThread*>(Sender);
    LOG_MSG( Format( "thread %s #http request body ",
        OPENARRAY(TVarRec,( FormatFloat( "#####", thread->ThreadID ).c_str() ) ) ).c_str() );
    LOG_PRINTF( Format( "%s\n", OPENARRAY(TVarRec,(thread->Url.c_str())) ).c_str() );
    LOG_PRINTF( Format( "%s\n", OPENARRAY(TVarRec,(body.c_str())) ).c_str() );
    //
    SetConsoleTextAttribute( hOut, DKBLUE );
    printf( "\nthread %d #http request\n", thread->ThreadID );
    printf( "%s\n%s\n", url.c_str(), body.c_str() );
    SetConsoleTextAttribute( hOut, NORMAL );
}
//------------------------------------------------------------------------------
void __fastcall PostToVtms( String data )
{
    TVtmsThread *vtms;
    vtms = new TVtmsThread( NULL, http_host, http_port );
    vtms->PostBody = data;
    vtms->OnHttpStatus = OnHttpStatus;
    vtms->OnHttpException = OnHttpException;
    vtms->OnHttpResponse = OnVtmsHttpResponse;
    vtms->BeforeHttpPost = BeforeVtmsHttpPost;
    vtms->OnTerminate = ((TCastClass*)NULL)->OnThreadTerminate;
    vtms->Resume();
}
//------------------------------------------------------------------------------
void PostAsyncSend( PPER_SOCKET_CONTEXT scktContext, char* buff, int len )
{
    PPER_IO_CONTEXT context = &scktContext->IOContext[1];
    //
    ZeroMemory( &context->Overlapped, sizeof(context->Overlapped) );
    context->IOOperation = CLIENT_IO_WRITE;
    //
    ZeroMemory( &context->sbuf, MAXLEN_CONTEXT_BUFF+1 );
    CopyMemory( &context->sbuf, buff, len );
    context->nTotalBytes = len;
    context->nSentBytes = 0;
    //
    context->wsabuf.buf = context->sbuf;
    context->wsabuf.len = context->nTotalBytes;
    //
    DWORD dwFlags = 0;
    DWORD dwSendNumBytes;
    int nRet = WSASend( scktContext->Socket,
                        &context->wsabuf, 1, &dwSendNumBytes,
                        dwFlags, &(context->Overlapped), NULL );
    if ( nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING) )
    {
        WSALastErrorFormatMessage( "[WSASend]" );
        CloseClient( scktContext );
    }
    else
    {
        scktContext->dwOutstandingOverlappedIO++;
    }
    //
    context = (void*)NULL;
}
//------------------------------------------------------------------------------
template<typename T>
void Remove( std::basic_string<T> & Str, const T * CharsToRemove )
{
    std::basic_string<T>::size_type pos = 0;
    while (( pos = Str.find_first_of( CharsToRemove, pos )) != std::basic_string<T>::npos )
    {
        Str.erase( pos, 1 );
    }
}

void ProcessData( char *data, int intStrLen, PPER_SOCKET_CONTEXT scktContext )
{
    static char last[100];
    TDateTime dt = Now();
    //
    std::string s = (data);
    Remove( s, "\r\n" );
    char *ptr = (char*)s.c_str();
    //
    LOG_MSG( Format("src=%s:%u #valid data=%s ",
        OPENARRAY(TVarRec,( scktContext->SockIp, GetPeerSocketPort(scktContext->Socket),
        ptr ) ) ).c_str() );
    //
    // YYYYMMDDHHNNSS$WAYBILL$CHECKPOINT$STATION$SEQ
    if ( CompareStr( last, String(ptr,strlen(ptr)) ) == 0 )
        InterlockedIncrement( &lngImgSeq );
    else
        lngImgSeq = 1;
    ZeroMemory( last, ARRAYSIZE( last ) );
    CopyMemory( last, ptr, strlen(ptr) );
    String tag = Format( "%s$%s$%s$%s$%s",
        OPENARRAY(TVarRec,( FormatDateTime( "YYYYMMDDHHNNSS", dt ),
        last, scktContext->Checkpoint, scktContext->Station, FormatFloat( "###", lngImgSeq ).c_str() )) );
    //
    //
    String line1 = FormatDateTime( "YYYY-MM-DD_HH:NN:SS", dt );
    String line2 = last;
    String line3 = scktContext->Station + "_" + scktContext->Checkpoint;
    //
    String host, snapshot_uri;
    int cam;
    for ( int i=0; i<scktContext->nvr_ip->Count; i++ ) //»P data cloner ip ÃöÁpªº NVR IP Address
    {
        host = scktContext->nvr_ip->Strings[i];
        cam = (int)scktContext->nvr_ip->Objects[i];
        String lines = Format( "%s\r\n%s\r\n%s\r\n", OPENARRAY( TVarRec,(line1.c_str(), line2.c_str(), line3.c_str() ) ) );
        snapshot( host, cam, tag, lines, tag + ".png" );
    }

    if ( vtms_enabled )
    {
        //VTMS
        //jd011960310000000012
        if ( strlen(ptr)==20 )
        {
            scktContext->root["id"] = Json::Value("");
            scktContext->root["piece_id"] = Json::Value(ptr);
        }
        else
            {
                scktContext->root["id"] = Json::Value(ptr);
                scktContext->root["piece_id"] = Json::Value("");
            }
        scktContext->root["seq"] = Json::Value((unsigned int)lngImgSeq);
        scktContext->root["station"] = Json::Value(scktContext->Station.c_str());
        scktContext->root["checkpoint"] = Json::Value(scktContext->Checkpoint.c_str());
        scktContext->root["source_ip"] = Json::Value(scktContext->SockIp);
        scktContext->root["time_stamp"] = Json::Value( Format( "%sT%s+08:00", OPENARRAY( TVarRec,
            (FormatDateTime( "YYYY-MM-DD", dt ).c_str(), FormatDateTime( "HH:NN:SS", dt ).c_str()) ) ).c_str() );
        //
        String body_a = scktContext->root.toStyledString().c_str();
        //String body_a = scktContext->fast_writer.write( scktContext->root ).c_str();
        PostToVtms( body_a.c_str() );
    }
}
//------------------------------------------------------------------------------
bool NonBlockingAccept( SOCKET listenSocket )
{
    fd_set fd;
    FD_ZERO( &fd );
    FD_SET( listenSocket, &fd );
    //
    struct timeval tv;
    tv.tv_sec = (ACCEPT_TIMEOUT_MS/1000);
    tv.tv_usec = (ACCEPT_TIMEOUT_MS%1000)*1000;

    unsigned long arg(1);
    ioctlsocket( listenSocket, FIONBIO, &arg ); //¶}/Ãö non-blocking ¼Ò¦¡
    //
    switch( select( FD_SETSIZE, &fd, NULL, NULL, &tv ) )
    {
    case 1:
        return true;
    case 0: //timeout - treat it like an error.
        return false;
    default: //error
        return false;
    }
}
//------------------------------------------------------------------------------
/*
void PrintHostSocket( SOCKET s )
{
    char name[128] = {0};
    gethostname( name, 128 );
    struct hostent *host = gethostbyname( name );
    if ( host == NULL )
    {
        return;
    }
    //
    struct sockaddr_in sa;
    ZeroMemory( &sa, sizeof( sa ) );

    int size = sizeof( sa );
    if ( getsockname( s, (SOCKADDR *)&sa, &size ) == SOCKET_ERROR )
        return;
    CopyMemory( &sa.sin_addr, host->h_addr_list[0], host->h_length );
    //
    listen_ip = (char*)inet_ntoa( sa.sin_addr );
    listen_port = (int)ntohs( sa.sin_port );
    LOG_MSG( Format( "#listening on %s:%d (%s) ",
              OPENARRAY(TVarRec,(
              (char*)inet_ntoa( sa.sin_addr ),
              (int)ntohs( sa.sin_port ), (char*)host->h_name
              ))).c_str() );

    printf( "\n#listening on %s:%u (%s)\n", (char*)inet_ntoa( sa.sin_addr ),
            (int)ntohs( sa.sin_port ), (char*)host->h_name );

    //String strHostIpPort = Format( "[%s:%u]", OPENARRAY(TVarRec,(
    //    inet_ntoa( sa.sin_addr ), (int)ntohs( sa.sin_port ) ) ) );
}
*/
//------------------------------------------------------------------------------
void __fastcall RetrieveClientList( void )
{
    EnterCriticalSection( &g_CriticalSection );
    SetConsoleTextAttribute( hOut, WHITE );
    printf( "\r\ttotal connection = %d\n", contextList->Count );
    for ( int i=0; i<contextList->Count; i++ )
    {
        if ( contextList->Items[i] )
        {
            printf( "\r\t(%3d) [%s:%u] Device Type=%s\n", i,
                    GetPeerSocketIp( contextList->Items[i]->Socket ),
                    GetPeerSocketPort( contextList->Items[i]->Socket ),
                    contextList->Items[i]->Name.c_str() );
        }
    }
    LeaveCriticalSection( &g_CriticalSection );
    //
    //
    DWORD dwUptime = (::GetTickCount() - lngElapsedTicks) / 1000;
    //unsigned int days = dwUptime / 86400;
    //dwUptime = dwUptime - (days * 86400);
    unsigned int hours = dwUptime / 3600;
    dwUptime = dwUptime - (hours * 3600);
    unsigned int minutes = dwUptime / 60;
    unsigned int seconds = dwUptime - (minutes*60);
    printf( "\a\t%02d:%02d:%02d has elapsed since system startup.\n",
        hours,minutes,seconds );
    //
    SetConsoleTextAttribute( hOut, NORMAL );
}
//------------------------------------------------------------------------------
// Console Event Handler
static BOOL WINAPI CtrlHandler( DWORD dwEvent )
{
    switch (dwEvent)
    {
    case CTRL_C_EVENT:
        {
            test_snapshot();
            //CaptureVideo( 2, tag );
            //RetrieveClientList();
        }
        break;
    case CTRL_BREAK_EVENT:
        g_bEndServer = true;
        break;
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_CLOSE_EVENT:
        g_bEndServer = true;
        break;

    default:
        // unknown type--better pass it on.
        return false;
    }
    return true;
}
//------------------------------------------------------------------------------
char* __fastcall ElapsedTime( void )
{
    static char result[10];
    DWORD dwUptime = (::GetTickCount() - lngElapsedTicks) / 1000;
    //unsigned int days = dwUptime / 86400;
    //dwUptime = dwUptime - (days * 86400);
    unsigned int hours = dwUptime / 3600;
    dwUptime = dwUptime - (hours * 3600);
    unsigned int minutes = dwUptime / 60;
    unsigned int seconds = dwUptime - (minutes*60);

    sprintf( result, "%d:%02d:%02d\0", hours,minutes,seconds );
    return result;
}
//------------------------------------------------------------------------------
bool __fastcall BelowToDataCloner( PPER_SOCKET_CONTEXT socket_context, String dc_ip )
{
    int intPosEqual, intLenValue;
    bool result = false;

    for ( int i=0; i<nvr->Count; i++ )
    {
        intPosEqual = AnsiPos( "=", nvr->Strings[i] );
        if ( intPosEqual == 0 )
            continue;
        intLenValue = nvr->Strings[i].Length();
        socket_context->List->Clear();
        socket_context->List->CommaText = nvr->Strings[i].SubString( intPosEqual+1, intLenValue - intPosEqual );
        if ( CompareStr( GetPeerSocketIp( socket_context->Socket ), socket_context->List->Strings[0] ) == 0 )
        {
            result = (CompareStr( nvr->Names[i], dc_ip ) == 0);
            break;
        }
    }

    return result;
}

//-----------------------------------------------------------------------------
int main( int argc, char *argv[] )
{
    TDate dateThen = TDate::CurrentDate();
    lngElapsedTicks = ::GetTickCount();
    lngAutoTicks = ::GetTickCount();
    //
    hOut = GetStdHandle( STD_OUTPUT_HANDLE );
    SetConsoleTextAttribute( hOut, NORMAL );
    //
    String strIniFileName = ExtractFilePath( ParamStr(0) ) + ParamStr(1);
    if ( ParamCount() < 1 )
        strIniFileName = ChangeFileExt( ExtractFilePath( ParamStr(0) ) +
                                        ExtractFileName( ParamStr(0) ), ".ini" );
    if ( !FileExists( strIniFileName ) )
    {
        LOG_MSG( "#configuration file not found. " );
        Application->Terminate();
    }
    //
    String strIniName = ChangeFileExt(ExtractFileName( strIniFileName ),EmptyStr);
    logger = new TLogger( ".\\log\\", strIniName.c_str() );
    //
    //
    TMemIniFile *iniFile = new TMemIniFile( strIniFileName );
    listen_ip = iniFile->ReadString( "Server", "ListenIp", EmptyStr );
    listen_port = iniFile->ReadInteger( "Server", "ListenPort", DEFAULT_LISTEN_PORT );
    lngIdleTimeoutMs = iniFile->ReadFloat( "Server", "IdleTimeoutMs", 0 );
    lngSnapshotDelayMs = iniFile->ReadInteger( "Server", "SnapshotDelayMs", 500 );
    bool blnIpDuplicateDisconnect = iniFile->ReadInteger( "Server", "IpDuplicateDisconnect", true );

    dc_client = new TStringList;   //Data Cloner
    iniFile->ReadSectionValues( "Client", dc_client );
    //
    nvr = new TStringList;
    iniFile->ReadSectionValues( "NVR", nvr );
    //
    http_host = iniFile->ReadString( "VTMS", "Host", "127.0.0.1" );
    http_port = iniFile->ReadInteger( "VTMS", "Port", 5000 );
    vtms_enabled = !http_host.IsEmpty();
    check_length = iniFile->ReadBool( "Server", "CheckLength", true );
    //
    autotest_enabled = iniFile->SectionExists( "AutoTest" );
    if ( autotest_enabled )
    {
        lngAutoTestIntervalMs = iniFile->ReadFloat( "AutoTest","IntervalMs", 1000 );
        if ( lngAutoTestIntervalMs == 0 )
            autotest_enabled = false;
        test_nvr = iniFile->ReadString( "AutoTest", "NVR", "127.0.0.1" );
        test_cam = iniFile->ReadInteger( "AutoTest", "Cam", 1 );
    }
    //
    delete iniFile;
    //
	int cur_major = 0, cur_minor = 0, cur_tiny = 0, cur_build = 0;
	GetVersionInfo(cur_major, cur_minor, cur_tiny, cur_build);
	String cur_version = Format("%d.%d.%d", OPENARRAY(TVarRec,
        (cur_major, cur_minor, cur_tiny)));

    SetConsoleTextAttribute( hOut, WHITE );
    puts( "Data Router - VTMS for DHL" );
    printf( "Version %s, Copyright (c) 2017 by Friendly Information Co.,LTD\n", cur_version.c_str() );
    //printf( "%s\n", String::StringOfChar( '-', 80 ).c_str() );
    //
    puts( "\n" );
    puts( "#Server" );
    if ( !listen_ip.IsEmpty() )
        printf( "Listen IP = %s\n", listen_ip );
    printf( "Listen Port = %d\n", listen_port );
    printf( "Idle Timeout = %s ms\n", FormatFloat( "#,##0", lngIdleTimeoutMs ).c_str() );
    printf( "Check Length = %s\n\n", check_length ? "yes" : "no" );
    //
    puts( "#Clients" );
    puts( dc_client->Text.c_str() );
    puts( "#NVR" );
    puts( nvr->Text.c_str() );
    if ( vtms_enabled )
    {
        puts( "#VTMS" );
        printf( "Host=%s\n", http_host );
        printf( "Port=%d\n", http_port );
    }
    //
    SetConsoleTextAttribute( hOut, NORMAL );
    //
    LOG_MSG( Format( "Start (%s) %s", OPENARRAY(TVarRec,(
        strIniFileName.c_str(),
        FormatFloat( "PID=######", GetProcessId( ExtractFileName( Application->ExeName ).c_str() ) ).c_str()
        ))).c_str() );
    printf( "\n#start (%s)\n", strIniFileName );
    //
    contextList = new TTypedList <PER_SOCKET_CONTEXT>( false );
    //
    SYSTEM_INFO systemInfo;
    GetSystemInfo( &systemInfo );
    unsigned long threadCount = systemInfo.dwNumberOfProcessors * 2;
    //unsigned long threadCount = systemInfo.dwNumberOfProcessors;
    //
    WSADATA wsaData;
    int nRet = WSAStartup(MAKEWORD(2,2), &wsaData);
    if ( nRet != 0 )
    {
        WSALastErrorFormatMessage( "[WSAStartup]" );
        return -1;
    }
    //
    // be able to gracefully handle CTRL-C and close handles
    if ( !SetConsoleCtrlHandler(CtrlHandler, true) )
    {
        LastErrorFormatMessage( "SetConsoleCtrlHandler" );
        WSACleanup();
        return -1;
    }
    //
    //
    PPER_SOCKET_CONTEXT lpPerSocketContext = NULL;
    unsigned long dwRecvNumBytes = 0;
    unsigned long dwFlags = 0;
    //
    InitializeCriticalSection( &g_CriticalSection );
    //
    SOCKET sdAccept;
    try
    {
        g_hIOCP = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 0 );
        if ( g_hIOCP == NULL )
        {
            LastErrorFormatMessage( "debug checkpoint 1" );
            return -1;
        }
        //
        unsigned long dwThreadId = 0;
        HANDLE hThread = NULL;
        for ( unsigned long i=0; i<threadCount; i++ )
        {
            // Create worker threads to service the overlapped I/O requests.
            // The decision to create 2 worker threads per CPU in the system is a heuristic.
            // Also, note that thread handles are closed right away,
            // because we will not need them and the worker threads will continue to execute.
            hThread = CreateThread( NULL, 0, WorkerThread, g_hIOCP, 0, &dwThreadId );
            if ( hThread == NULL )
            {
                LastErrorFormatMessage( "debug checkpoint 2" );
                return -1;
            }
            else
                g_ThreadHandles[i] = hThread;
        }
        //
        //
        if ( !CreateListenSocket( listen_ip, listen_port ) )
        {
            return -1;
        }
        //
        while ( !g_bEndServer )
        {
            if ( NonBlockingAccept( g_sdListen ) )
            {
                // Loop forever accepting connections from clients until console shuts down.
                sdAccept = accept(g_sdListen, NULL, NULL);
                if ( sdAccept == INVALID_SOCKET )
                {
                    WSALastErrorFormatMessage( "[WSAccept]" );
                    return -1;
                }
            }
            else
            {
                if ( dateThen <= TDate( Yesterday() ) ) //´«¤é
                {
                    lngImgSeq = 1;
                    LOG_MSG( Format( "#´«¤é %s ", OPENARRAY(TVarRec,(TDate::CurrentDate().DateTimeString().c_str())) ).c_str() );
                    dateThen = TDate::CurrentDate();
                }
                //
                //printf( "\r[%s:%d] %s", listen_ip.c_str(), listen_port, ElapsedTime() );
                //printf( "\rtotal %d clients - %s elapsed", contextList->Count, ElapsedTime() );
                printf( "\r> %s", ElapsedTime() );

                SetConsoleTitle( Format( "%s (%u/%u)", OPENARRAY(TVarRec,
                    (strIniName.c_str(),contextList->Count,dc_client->Count) )).c_str() );
                //
                if ( contextList->Count )
                {
                    if ( ++intIndex >= contextList->Count )
                        intIndex = 0;
                    //
                    if ( lngIdleTimeoutMs )
                    {
                        EnterCriticalSection( &g_CriticalSection );
                        if ( ::GetTickCount() - contextList->Items[intIndex]->Ticks >= lngIdleTimeoutMs )
                        {   //±j­¢Â_½u
                        //access violation
                        /*
                            LOG_MSG(
                                Format( "src=%s:%u #connection timeout.",
                                OPENARRAY(TVarRec,(
                                    GetPeerSocketIp( contextList->Items[intIndex]->Socket ),
                                    GetPeerSocketPort( contextList->Items[intIndex]->Socket ) ))).c_str() );
                            printf( "\n%src=%s:%u #connection timeout.\n",
                                GetPeerSocketIp( contextList->Items[intIndex]->Socket ),
                                GetPeerSocketPort( contextList->Items[intIndex]->Socket ) );
                            //
                        */
                            CloseClient( contextList->Items[intIndex] );
                        }
                        LeaveCriticalSection( &g_CriticalSection );
                    }
                }
                continue;
            }
            //
            //
            //¨Ï¥Î¨Ó¦Û localhost(127.0.0.1) ªº Socket connection °±¤î service
            //if ( strcmp( GetPeerSocketIp( sdAccept ), "127.0.0.1" ) == 0 )
            //    break;
            LOG_MSG( Format( "src=%s:%u #connected,total=%u",
                              OPENARRAY(TVarRec,(
                              GetPeerSocketIp( sdAccept ),
                              GetPeerSocketPort( sdAccept ),
                              contextList->Count ))).c_str() );

            printf( "\nsrc=%s:%u #connected\n",
                    GetPeerSocketIp( sdAccept ), GetPeerSocketPort( sdAccept ) );
            //
            if ( blnIpDuplicateDisconnect )
            {
                EnterCriticalSection( &g_CriticalSection );
                bool blnDuplicated = false;
                char ip1[MAXLEN_SOCKIPPORT] = {0};
                char ip2[MAXLEN_SOCKIPPORT] = {0};
                //sprintf( ip2, "%s:%d", GetPeerSocketIp(sdAccept), GetPeerSocketPort(sdAccept) ); //ip:port
                sprintf( ip2, "%s", GetPeerSocketIp(sdAccept) );
                for ( int i=0; i<contextList->Count; i++ )
                {   //ÀË¬d³s½u¤W¨ÓªºIP¬O§_¦b contextList ¤º?
                    SOCKET s = contextList->Items[i]->Socket;
                    //sprintf( ip1, "%s:%d", GetPeerSocketIp(s), GetPeerSocketPort(s) );    //ip:port
                    sprintf( ip1, "%s", GetPeerSocketIp(s) );
                    if ( strcmp( ip1, ip2 ) == 0 )
                    {
                        LOG_MSG( Format(
                                      "src=%s:%u #duplicate IP address ",
                                      OPENARRAY(TVarRec,(
                                      GetPeerSocketIp( sdAccept ),
                                      GetPeerSocketPort( sdAccept ) ) ) ).c_str() );
                        SetConsoleTextAttribute( hOut, RED );
                        printf( "\nsrc=%s:%u #duplicate IP address\n",
                                GetPeerSocketIp( sdAccept ), GetPeerSocketPort( sdAccept ) );
                        SetConsoleTextAttribute( hOut, NORMAL );
                        blnDuplicated = true;
                        int nRet = closesocket( sdAccept );   //±j­¢Â_½u
                        if ( nRet == SOCKET_ERROR )
                            WSALastErrorFormatMessage( "[closesocket]" );
                        break;
                    }
                }
                LeaveCriticalSection( &g_CriticalSection );
                if ( blnDuplicated == true )
                    continue;
            }
            //
            // we add the just returned socket descriptor to the IOCP along with its
            // associated key data.  Also the global list of context structures
            // (the key data) gets added to a global list.
            try
            {
                EnterCriticalSection( &g_CriticalSection );
                //to avoid a race condition that SIO_GET_QOS is added, and client resets, so it completes
                // with an error code. The workthreads sees that outstanding I/O count is zero, then frees
                // the lpPerSocketContext
                lpPerSocketContext = UpdateCompletionPort( sdAccept );
                if ( lpPerSocketContext == NULL )
                {
                    return -1;
                }
                //String client_ip = GetPeerSocketIp( lpPerSocketContext->Socket );
                //
                // post initial receive on this socket
                // µo¥X²Ä¤@­ÓWSARecv,¦]¬°¬O«D¦P¨B¨ç¦¡,©Ò¥H¤£·|block
                dwFlags = 0;
                nRet = WSARecv( sdAccept,
                            &(lpPerSocketContext->IOContext[0].wsabuf),
                            1, &dwRecvNumBytes, &dwFlags,
                            &(lpPerSocketContext->IOContext[0].Overlapped), NULL );
                if ( nRet == SOCKET_ERROR && (WSAGetLastError()!=ERROR_IO_PENDING) )
                {
                    WSALastErrorFormatMessage( "[WSARecv(init)]" );
                    CloseClient(lpPerSocketContext);
                    //
                    continue;
                }
                lpPerSocketContext->dwOutstandingOverlappedIO++;
            }
            __finally
            {
                LeaveCriticalSection(&g_CriticalSection);
            }
        } //while
    }
    __finally
    {
        // Cause worker threads to exit
        if ( g_hIOCP )
        {
            //GetLocalTime( &time );
            for ( unsigned long i=0; i<threadCount; i++ )
                PostQueuedCompletionStatus( g_hIOCP, 0, NULL, NULL );
        }
        //
        //Make sure worker threads exits.
        if ( WaitForMultipleObjects( threadCount, g_ThreadHandles, true, 3000 )
                != WAIT_OBJECT_0 )
        {
            LastErrorFormatMessage( "debug checkpoint 4" );
        }
        else
            for ( unsigned long i=0; i<threadCount; i++ )
            {
                if ( g_ThreadHandles[i] != INVALID_HANDLE_VALUE )
                    CloseHandle(g_ThreadHandles[i]);
                g_ThreadHandles[i] = INVALID_HANDLE_VALUE;
            }
        //
        CtxtListFree();
        //
        if ( g_hIOCP )
        {
            CloseHandle(g_hIOCP);
            g_hIOCP = NULL;
        }
        //
        if ( g_sdListen != INVALID_SOCKET )
        {
            closesocket(g_sdListen);
            g_sdListen = INVALID_SOCKET;
        }
        //
        LOG_MSG( "#shutdown" );
        printf( "\n#shutdown\n" );
    }   //finally
    //
    DeleteCriticalSection(&g_CriticalSection);
    WSACleanup();

    delete dc_client;
    delete contextList;
    delete nvr;

    if ( logger )
        delete logger;

    return 1;
} //main
//------------------------------------------------------------------------------
//  Create a listening socket.
#ifndef SO_EXECLUSINEADDRUSE
#define SO_EXECLUSINEADDRUSE ((int)(~SO_REUSEADDR))
#endif

bool CreateListenSocket( String listenIP, int listenPort )
{
    g_sdListen = WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED );
    if ( g_sdListen == INVALID_SOCKET )
    {
        WSALastErrorFormatMessage( "#WSASocket" );
        return false;
    }
    //
    //
    SOCKADDR_IN si_addrlocal;
    ZeroMemory( &si_addrlocal, sizeof(SOCKADDR_IN) );
    //
    si_addrlocal.sin_family = AF_INET;
    si_addrlocal.sin_port = htons( listenPort );
    if ( listenIP.IsEmpty() )
        si_addrlocal.sin_addr.s_addr = htonl( INADDR_ANY );
    else
        si_addrlocal.sin_addr.s_addr = inet_addr( listenIP.c_str() );
    ZeroMemory( &si_addrlocal.sin_zero, sizeof(si_addrlocal.sin_zero) );
    //
    int nRet;
    bool val = true;
    nRet = setsockopt( g_sdListen, SOL_SOCKET, SO_EXECLUSINEADDRUSE, (char*)&val, sizeof(val) );
    if ( nRet == SOCKET_ERROR )
    {
        WSALastErrorFormatMessage( "#setsockopt" );
        return false;
    }
    //
    nRet = bind( g_sdListen, (struct sockaddr *)&si_addrlocal, sizeof(si_addrlocal) );
    if ( nRet == SOCKET_ERROR )
    {
        WSALastErrorFormatMessage( "#bind" );
        return false;
    }
    else
    {   //get bind IP Address
        char client_host[NI_MAXHOST];  //The clienthost will hold the IP address.
        char client_service[NI_MAXSERV];
        int error = getnameinfo( (struct sockaddr *)&si_addrlocal,
            sizeof(si_addrlocal), client_host, sizeof(client_host), client_service,
            sizeof(client_service), NI_NUMERICHOST|NI_NUMERICSERV);
        if( error == 0 )
        {
            LOG_MSG( Format( "#host IP address: %s", OPENARRAY(TVarRec,(client_host))).c_str() );
            printf( "\n#host IP address %s", client_host );
            LOG_MSG( Format( "#listening on port %s", OPENARRAY(TVarRec,(client_service))).c_str() );
            printf( "\n#listening on port %s\n", client_service );
        }
    }

    nRet = listen( g_sdListen, 5 );
    if ( nRet == SOCKET_ERROR )
    {
        WSALastErrorFormatMessage( "#listen" );
        return false;
    }
    //
    // Disable send buffering on the socket.  Setting SO_SNDBUF
    // to 0 causes winsock to stop bufferring sends and perform
    // sends directly from our buffers, thereby reducing CPU usage.
    int nZero = 0;
    nRet = setsockopt( g_sdListen, SOL_SOCKET, SO_SNDBUF, (char *)&nZero, sizeof(nZero) );
    if ( nRet == SOCKET_ERROR )
    {
        WSALastErrorFormatMessage( "#setsockopt(SNDBUF)" );
        return false;
    }
    //
    LINGER lingerStruct;
    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    nRet = setsockopt( g_sdListen, SOL_SOCKET, SO_LINGER,
                       (char *)&lingerStruct, sizeof(lingerStruct) );
    if ( nRet == SOCKET_ERROR )
    {
        WSALastErrorFormatMessage( "#setsockopt(SO_LINGER)" );
        return false;
    }
    //PrintHostSocket( g_sdListen );
    //
    return true;
}
//------------------------------------------------------------------------------
// Worker thread that handles all I/O requests on any socket handle added to the IOCP.
//
unsigned long WINAPI WorkerThread( LPVOID WorkThreadContext )
{
    HANDLE hIOCP = (HANDLE)WorkThreadContext;

    PPER_SOCKET_CONTEXT lpPerSocketContext = NULL;
    LPOVERLAPPED lpOverlapped = NULL;
    PPER_IO_CONTEXT lpIOContext;
    SYSTEMTIME time;

    unsigned long dwRecvNumBytes = 0;
    unsigned long dwSendNumBytes = 0;
    unsigned long dwFlags = 0;
    unsigned long dwIoSize;
    bool bSuccess = false;
    String data;
    int nRet;
    String strSize;

    while ( true )
    {
        // continually loop to service io completion packets
        bSuccess = GetQueuedCompletionStatus( hIOCP, &dwIoSize,
                                              (PDWORD_PTR)&lpPerSocketContext,
                                              &lpOverlapped, INFINITE );
        if ( !bSuccess )
        {
            LastErrorFormatMessage( "GetQueueCompletionStatus" );
        }
        //
        if ( lpPerSocketContext == NULL )
        {
            // PostQueuedCompletionStatus post an I/O packet with
            // a NULL CompletionKey (or if we get one for any reason).
            // It is time to exit.
            return 0;
        }
        //
        EnterCriticalSection( &g_CriticalSection );
        lpPerSocketContext->dwOutstandingOverlappedIO--;
        //printf( "OverlappedIO count=%d\n", lpPerSocketContext->dwOutstandingOverlappedIO );
        LeaveCriticalSection( &g_CriticalSection );
        //
        if ( !bSuccess || (bSuccess && (dwIoSize==0)) )
        {
            //dwIOSize == 0 indicates that the socket has been closed by the peer.

            // client connection dropped, continue to service remaining
            // (and possibly new) client connections
            CloseClient( lpPerSocketContext ); //REMOTE DISCONNECT
            continue;
        }
        //
        if ( lngIdleTimeoutMs )
            lpPerSocketContext->Ticks = ::GetTickCount(); //reset ticks
        //
        // determine what type of IO packet has completed by checking the PER_IO_CONTEXT
        // associated with this socket.  This will determine what action to take.
        lpIOContext = (PPER_IO_CONTEXT)lpOverlapped;
        switch( lpIOContext->IOOperation )
        {
        case CLIENT_IO_READ:
            {
                EnterCriticalSection( &g_CriticalSection );
                lpIOContext->rbuf[dwIoSize] = 0;
                //lpIOContext->Buff = ¦¬¨ìªº¸ê®Æ
                //dwIoSize = ¸ê®Æªø«×
                strSize = FormatFloat( "#,##0", dwIoSize );
                ZeroMemory( dump_buff, DUMP_BUFF_SIZE*10 );
                LOG_MSG( Format("src=%s:%u bytes=%s ",
                        OPENARRAY(TVarRec,(
                        GetPeerSocketIp( lpPerSocketContext->Socket ),
                        GetPeerSocketPort( lpPerSocketContext->Socket ),
                        strSize ) )).c_str() );
                LOG_DUMP2( lpIOContext->rbuf, dump_buff, dwIoSize );
                //
                if ( dc_client->IndexOfName( GetPeerSocketIp( lpPerSocketContext->Socket ) ) != -1 )
                {   //¨Ó¦Û Data Cloner ªº¸ê®Æ
                    SetConsoleTextAttribute( hOut, BLUE );
                    printf( "\r\nsrc=%s:%u bytes=%s\r\n%s",
                        GetPeerSocketIp( lpPerSocketContext->Socket ),
                        GetPeerSocketPort( lpPerSocketContext->Socket ),
                        strSize, dump_buff );
                    SetConsoleTextAttribute( hOut, NORMAL );
                    //
                    for ( unsigned long index=0; index<dwIoSize; index++ )
                        lpPerSocketContext->Buff->push_back( lpIOContext->rbuf[index] );
                    //
                    int data_len;
                    std::vector<char>::iterator begin,pos;
                    begin = lpPerSocketContext->Buff->begin();
                    do
                    {
                        pos = find( begin, lpPerSocketContext->Buff->end(), 0x0A );
                        if ( pos != lpPerSocketContext->Buff->end() )
                        {
                            bool valid = true;
                            std::string str( begin, pos );
                            data = String( str.c_str() );
                            //data = ¥h±¼ 0X0A µ²§Àªº¦r¦ê
                            data_len = data.Length();
                            if ( data_len > 1 )
                            {
                                SetConsoleTextAttribute( hOut, YELLOW );
                                if ( data_len == 21 )
                                {
                                    data.Delete( 1,1 ); //delete first character
                                    printf( "#¦³®Äªº¸ê®Æ (PIECE ID)=%s\n", data.c_str() );
                                }
                                else
                                    if ( data_len == 10 )
                                        printf( "#¦³®Äªº¸ê®Æ (HAWB)=%s\n", data.c_str() );
                                    else
                                    {
                                        if ( check_length )
                                        {
                                            valid = false;
                                            SetConsoleTextAttribute( hOut, RED );
                                            printf( "#µL®Äªº¸ê®Æ=%s\n", data.c_str() );
                                        }
                                        else
                                            printf( "#¤wÅª¨ú¸ê®Æ=%s\n", data.c_str() );
                                    }

                                SetConsoleTextAttribute( hOut, NORMAL );
                                //
                                if ( valid )
                                    ProcessData( data.c_str(), data_len, lpPerSocketContext );
                            }
                            //
                            lpPerSocketContext->Buff->erase( lpPerSocketContext->Buff->begin(), pos );
                            begin = pos+1;
                        }
                        else
                            break;
                    } while (1);
                }
                //
                //Post WSARecv to complete the operation.
                ZeroMemory( &(lpIOContext->Overlapped), sizeof(lpIOContext->Overlapped) );
                lpIOContext->IOOperation = CLIENT_IO_READ;
                lpIOContext->wsabuf.buf = lpIOContext->rbuf;
                lpIOContext->wsabuf.len = MAXLEN_CONTEXT_BUFF;
                dwRecvNumBytes = 0;
                dwFlags = 0;
                nRet = WSARecv( lpPerSocketContext->Socket,
                                &lpIOContext->wsabuf, 1, &dwRecvNumBytes,
                                &dwFlags, &lpIOContext->Overlapped, NULL );
                if ( nRet==SOCKET_ERROR && (WSAGetLastError()!=ERROR_IO_PENDING) )
                {
                    WSALastErrorFormatMessage( "[WSARecv]" );
                    CloseClient(lpPerSocketContext);
                }
                else
                {
                    lpPerSocketContext->dwOutstandingOverlappedIO++;
                }
                LeaveCriticalSection( &g_CriticalSection );
            }
            //
            break;

        case CLIENT_IO_WRITE:
            {
                try
                {
                    EnterCriticalSection( &g_CriticalSection );
                    strSize = FormatFloat( "#,##0", dwIoSize );
                    ZeroMemory( dump_buff, DUMP_BUFF_SIZE*10 );
                    LOG_MSG( Format("dst=%s:%u bytes=%s",
                                 OPENARRAY(TVarRec,(
                                 GetPeerSocketIp( lpPerSocketContext->Socket ),
                                 GetPeerSocketPort( lpPerSocketContext->Socket ),
                                 strSize ) ) ).c_str() );
                    LOG_DUMP2( lpIOContext->sbuf, dump_buff, dwIoSize );
                    //
                    SetConsoleTextAttribute( hOut, GREEN );
                    printf( "\ndst=%s:%u bytes=%s\r\n%s\n",
                        GetPeerSocketIp( lpPerSocketContext->Socket ),
                        GetPeerSocketPort( lpPerSocketContext->Socket ),
                        strSize, dump_buff );
                    SetConsoleTextAttribute( hOut, NORMAL );
                    //
                    ZeroMemory( &(lpIOContext->Overlapped), sizeof(lpIOContext->Overlapped) );
                    lpIOContext->IOOperation = CLIENT_IO_WRITE;
                    lpIOContext->nSentBytes += dwIoSize;
                    //printf( "\nSent:Total = %d:%d\n", lpIOContext->nSentBytes, lpIOContext->nTotalBytes );

                    // the previous write operation didn't send all the data,
                    // post another send to complete the operation
                    if ( lpIOContext->nSentBytes < lpIOContext->nTotalBytes )
                    {
                        lpIOContext->wsabuf.buf = lpIOContext->sbuf + lpIOContext->nSentBytes;
                        lpIOContext->wsabuf.len = lpIOContext->nTotalBytes - lpIOContext->nSentBytes;
                        //
                        dwSendNumBytes = 0;
                        dwFlags = 0;
                        nRet = WSASend( lpPerSocketContext->Socket,
                                    &lpIOContext->wsabuf, 1, &dwSendNumBytes,
                                    dwFlags, &(lpIOContext->Overlapped), NULL );
                        if ( nRet==SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING) )
                        {
                            WSALastErrorFormatMessage( "[WSASend]" );
                            CloseClient( lpPerSocketContext );
                        }
                        else
                        {
                            lpPerSocketContext->dwOutstandingOverlappedIO++;
                        }
                    }
                }
                __finally
                {
                    LeaveCriticalSection( &g_CriticalSection );
                }
            }
            break;
            //
        default:
            //Unknown operation
            break;
        } //switch
    } //while
    return 0;
}
//------------------------------------------------------------------------------
// Allocate a socket context for the new connection.
//
PPER_SOCKET_CONTEXT CtxtAllocate( SOCKET sd )
{
    int intPosEqual, intLenValue;
    PPER_SOCKET_CONTEXT result = NULL;
    try
    {
        EnterCriticalSection( &g_CriticalSection );
        //
        result = (PPER_SOCKET_CONTEXT) HeapAlloc( GetProcessHeap(),
                 HEAP_ZERO_MEMORY, sizeof(PER_SOCKET_CONTEXT) );
        if ( result )
        {
            ZeroMemory( result->SockIp, MAXLEN_SOCKIPPORT );
            //wsprintf( result->SockIp, "%s:%d\0", GetPeerSocketIp(sd), GetPeerSocketPort(sd) ); //ip:port
            wsprintf( result->SockIp, "%s\0", GetPeerSocketIp(sd) );
            //
            result->IOContext[0].Overlapped.Internal = 0;
            result->IOContext[0].Overlapped.InternalHigh = 0;
            result->IOContext[0].Overlapped.Offset = 0;
            result->IOContext[0].Overlapped.OffsetHigh = 0;
            result->IOContext[0].Overlapped.hEvent = NULL;
            result->IOContext[0].IOOperation = CLIENT_IO_READ;
            result->IOContext[0].nTotalBytes = 0;
            result->IOContext[0].nSentBytes = 0;
            result->IOContext[0].wsabuf.buf = result->IOContext[0].rbuf;
            result->IOContext[0].wsabuf.len = MAXLEN_CONTEXT_BUFF;
            //
            result->IOContext[1].Overlapped.Internal = 0;
            result->IOContext[1].Overlapped.InternalHigh = 0;
            result->IOContext[1].Overlapped.Offset = 0;
            result->IOContext[1].Overlapped.OffsetHigh = 0;
            result->IOContext[1].Overlapped.hEvent = NULL;
            result->IOContext[1].IOOperation = CLIENT_IO_WRITE;
            result->IOContext[1].nTotalBytes = 0;
            result->IOContext[1].nSentBytes = 0;
            result->IOContext[1].wsabuf.buf = result->IOContext[1].sbuf;
            result->IOContext[1].wsabuf.len = MAXLEN_CONTEXT_BUFF;
            //
            result->Socket = sd;
            result->Ticks = ::GetTickCount();
            result->Buff = new vector<char>;
            //
            result->List = new TStringList;
            result->List->Clear();
            //
            result->nvr_ip = new TStringList;
            //
            if ( dc_client->IndexOfName( result->SockIp ) != -1 )
            {   //³o¨Ç¬O Data Cloner ªº³]³Æ³s½u
                result->Name = "Data Cloner";
                result->List->CommaText = dc_client->Values[result->SockIp];
                result->Machine = result->List->Strings[0];
                result->Checkpoint = result->List->Strings[1];
                result->Station = result->List->Strings[2];
                //
                for ( int i=0; i<nvr->Count; i++ )
                {
                    if ( CompareStr( nvr->Names[i], result->SockIp ) == 0 )
                    {
                        intPosEqual = AnsiPos( "=", nvr->Strings[i] );
                        if ( intPosEqual != 0 )
                        {
                            intLenValue = nvr->Strings[i].Length();
                            //
                            result->List->Clear();
                            result->List->CommaText = nvr->Strings[i].SubString( intPosEqual+1, intLenValue - intPosEqual );
                            result->nvr_ip->AddObject( result->List->Strings[0], (TObject*)result->List->Strings[1].ToIntDef(0) );
                        }
                    }
                }
            }
        }
        else
        {
            LastErrorFormatMessage( "debug checkpoint 5" );
        }
    }
    __finally
    {
        LeaveCriticalSection( &g_CriticalSection );
    }

    return result;
}
//------------------------------------------------------------------------------
//  Allocate a context structures for the socket and add the socket to the IOCP.
//  Additionally, add the context structure to the global list of context structures.
//
PPER_SOCKET_CONTEXT UpdateCompletionPort( SOCKET sd )
{
    PPER_SOCKET_CONTEXT result = CtxtAllocate( sd );
    if ( result == NULL )
        return NULL;

    g_hIOCP = CreateIoCompletionPort( (HANDLE)sd, g_hIOCP, (DWORD_PTR)result, 0 );
    if ( g_hIOCP == NULL )
    {
        LastErrorFormatMessage( "debug checkpoint 6" );
        HeapFree( GetProcessHeap(), 0, result );
        return NULL;
    }

    try
    {
        EnterCriticalSection( &g_CriticalSection );
        contextList->Add( result );
    }
    __finally
    {
        LeaveCriticalSection( &g_CriticalSection );
    }
    //
    //
    return result;
}

//------------------------------------------------------------------------------
//  Close down a connection with a client. This involves closing the socket
//  (when initiated as a result of a CTRL-C the socket closure is not graceful).
//  Additionally, any context data associated with that socket is free'd.
//
void CloseClient( PPER_SOCKET_CONTEXT lpPerSocketContext )
{
    SYSTEMTIME time;
    bool blnGraceful = false;

    try
    {
        EnterCriticalSection( &g_CriticalSection );
        //
        if ( lpPerSocketContext )
        {
            if ( lpPerSocketContext->Socket != INVALID_SOCKET )
            {
                LOG_MSG( Format(
                              "src=%s:%u #disconnected ",
                              OPENARRAY(TVarRec,(
                              GetPeerSocketIp( lpPerSocketContext->Socket ),
                              GetPeerSocketPort( lpPerSocketContext->Socket ) ) ) ).c_str() );

                printf( "\nsrc=%s:%u #disconnected\n",
                        GetPeerSocketIp( lpPerSocketContext->Socket ),
                        GetPeerSocketPort( lpPerSocketContext->Socket ) );
                //
                if ( !blnGraceful )
                {
                    // force the subsequent closesocket to be abortative.
                    LINGER lingerStruct;
                    lingerStruct.l_onoff = 1;
                    lingerStruct.l_linger = 0;
                    setsockopt( lpPerSocketContext->Socket, SOL_SOCKET, SO_LINGER,
                                (char *)&lingerStruct, sizeof(lingerStruct) );
                }
                //
                closesocket( lpPerSocketContext->Socket );
                lpPerSocketContext->Socket = INVALID_SOCKET;
            }
            CtxtListDeleteFrom( lpPerSocketContext );
        }
        else
        {
            //SetConsoleTextAttribute( hOut, RED );
            //SetConsoleTextAttribute( hOut, NORMAL );
        }
    }
    __finally
    {
        LeaveCriticalSection( &g_CriticalSection );
    }
}
//------------------------------------------------------------------------------
void CtxtListDeleteFrom( PPER_SOCKET_CONTEXT lpPerSocketContext )
{
    try
    {
        EnterCriticalSection( &g_CriticalSection );
        //
        if ( !g_bEndServer && (lpPerSocketContext->dwOutstandingOverlappedIO > 0) )
        {
            return;
        }
        //
        if ( lpPerSocketContext )
        {
            vector<char> *p = lpPerSocketContext->Buff;
            delete p;
            lpPerSocketContext->Buff = (void*)NULL;
            //
            TStringList *list = lpPerSocketContext->List;
            delete list;
            lpPerSocketContext->List = (void*)NULL;
            //
            TStringList *nvr_ip = lpPerSocketContext->nvr_ip;
            for ( int i=0; i<nvr_ip->Count; i++ )
                (TObject*)lpPerSocketContext->nvr_ip->Objects[i] = (void*)NULL;
            delete nvr_ip;
            lpPerSocketContext->nvr_ip = (void*)NULL;
            //
            PPER_SOCKET_CONTEXT temp = lpPerSocketContext;
            contextList->Remove( lpPerSocketContext );
            lpPerSocketContext = (void*)NULL;
            HeapFree( GetProcessHeap(), 0, temp );
            temp = (void*)NULL;
        }
        else
        {
            //SetConsoleTextAttribute( hOut, RED );
            //SetConsoleTextAttribute( hOut, NORMAL );
        }
    }
    __finally
    {
        LeaveCriticalSection( &g_CriticalSection );
    }
}
//------------------------------------------------------------------------------
void CtxtListFree( void )
{
    try
    {
        EnterCriticalSection( &g_CriticalSection );
        //
        g_bEndServer = true;
        for ( int i=contextList->Count-1; i>=0; i-- )
            CloseClient( contextList->Items[i] );
    }
    __finally
    {
        LeaveCriticalSection( &g_CriticalSection );
    }
}
//------------------------------------------------------------------------------



