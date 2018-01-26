// Logger.cpp: implementation of the Logger class.
//
//////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <SysUtils.hpp>

#define __LOGGER_CPP__
    #include "Logger.h"
#undef __LOGGER_CPP__

#pragma hdrstop

#pragma package(smart_init)
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------------
TLogger::TLogger( char *path, char *prefix = NULL )	//default parameter 2 = NULL
{
    char mutexName[256+256] = { "MUTEX.LOGGER.\0" };
    int len = strlen( mutexName );
    //
    char buff[ _MAX_PATH  ] = { '\0' };
    char drive[ _MAX_DRIVE ] = { '\0' };
    char dir[ _MAX_DIR ] = { '\0' };
    char ext[ _MAX_EXT ] = { '\0' };
	//
	ZeroMemory( filename_prefix, _MAX_FNAME );
	if ( prefix != NULL )
		CopyMemory( filename_prefix, prefix, strlen(prefix) );
    //
    GetModuleFileName( GetModuleHandle( NULL ), buff, _MAX_PATH );
    ZeroMemory( arrFilename, _MAX_FNAME+1 );
    _splitpath( buff, drive, dir, arrFilename, ext );
    CopyMemory( &mutexName[len], arrFilename, strlen(arrFilename) );
    //
    ZeroMemory( arrPath, _MAX_PATH+1 );
    if ( path == NULL )
        _makepath( arrPath, drive, dir, NULL, NULL );
    else
        CopyMemory( arrPath, path, lstrlen( path ) );
    //
    if ( !CreateDirectory( arrPath, NULL ) )
	{
		//				
	}
    //
    hMutex = OpenMutex( MUTEX_ALL_ACCESS, false, mutexName );
    if ( hMutex == NULL )
    {
        hMutex = CreateMutex( 0, false, mutexName );
        NewLogFile();
    }
    //
    hFile = CreateFile( arrFullFilename, GENERIC_READ|GENERIC_WRITE,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );

    InitializeCriticalSection( &hLock );
}
//---------------------------------------------------------------------------
TLogger::~TLogger()
{
    CloseLog();
}
//---------------------------------------------------------------------------
void TLogger::CloseLog( void )
{
    CloseHandle( hFile );
    //
    DeleteCriticalSection( &hLock );
    ReleaseMutex( hMutex );
    CloseHandle( hMutex );
}
//---------------------------------------------------------------------------
const int limitInKB = 256;                 // 256 KBytes

void TLogger::Shrink( void )
{
    DWORD size = GetFileSize( hFile, NULL );
    if ( size == 0xFFFFFFFF )
        return;
    //
    //
    size /= 1024;
    if (size > limitInKB) //Log file too large, shrink it...
    {
        int curReadPos = (int)(size*1024-(limitInKB*1024)*0.9); //New log size is 10% smaller than the set limit
        int curWritePos = 0;
        const int bufsize = 16384; // 16KB
        char buffer[bufsize];
        DWORD numread;
        DWORD numwritten;
        BOOL bFirst = true;
        do
        {
            SetFilePointer( hFile, curReadPos, 0, FILE_BEGIN );
            if (!ReadFile(hFile, buffer, bufsize, &numread, 0))
                break;
            curReadPos += numread;

            SetFilePointer(hFile, curWritePos, 0, FILE_BEGIN);
            if (bFirst) //Assure log starts with complete line
            {
                unsigned int i;
                for (i=0; i<numread; i++)
                {
                    if (buffer[i] == '\n')
                        break;
                }
                if (i >= (numread-1))
                    continue;
                bFirst = false;
                if (!WriteFile(hFile, buffer + i + 1, numread - i - 1, &numwritten, 0))
                    break;
            }
            else
                if (!WriteFile(hFile, buffer, numread, &numwritten, 0))
                    break;
            curWritePos += numwritten;

        } while (numread == bufsize);
        //
        SetFilePointer(hFile, curWritePos, 0, FILE_BEGIN);
        SetEndOfFile(hFile);
    }
}
//-----------------------------------------------------------------------
void TLogger::LastError( void )
{
    EnterCriticalSection( &hLock );
    //
    char* lpMsgBuf = NULL;
    DWORD errCode = GetLastError();
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   errCode,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                   (LPTSTR) &lpMsgBuf,
                   0,
                   NULL );

    int len = lstrlen(lpMsgBuf)-1;
    if ( len > 0 )
    {
        while ( len>=0 && (lpMsgBuf[len]==LINE_CR || lpMsgBuf[len]==LINE_LF) )
            len--;
        lpMsgBuf[len+1] = 0;
		//
        WriteBuffer( (char*)lpMsgBuf );
		//
        LocalFree( lpMsgBuf ); // Free the buffer.
    }
    //
    LeaveCriticalSection( &hLock );
}
//---------------------------------------------------------------------------
void TLogger::NewLogFile( void )
{
    GetLocalTime( &time );
	if ( strlen( filename_prefix ) )
		wsprintf( arrFullFilename, "%s%s%04d%02d%02d.log", arrPath, filename_prefix, time.wYear, time.wMonth, time.wDay );
	else
		wsprintf( arrFullFilename, "%s%s-%04d%02d%02d.log", arrPath, arrFilename, time.wYear, time.wMonth, time.wDay );
    //
    year = time.wYear;
    month = time.wMonth;
    day = time.wDay;
    //
    HANDLE hFile = CreateFile( arrFullFilename, GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0 );
    //
    if ( hFile == INVALID_HANDLE_VALUE )
        LastError();
    //else
        //WriteHeader();
     
    CloseHandle( hFile );
}
//------------------------------------------------------------------------------
/*
	2012-02-20 01:46:51.065 message
*/
void TLogger::Message( char *msg )
{
    EnterCriticalSection( &hLock );
    //
    GetLocalTime( &time );
    static char timeStamp[30] = {0};
    wsprintf( timeStamp, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] \0",
              time.wYear, time.wMonth, time.wDay,
              time.wHour, time.wMinute, time.wSecond, time.wMilliseconds );
    if ( (time.wYear != year) || (time.wMonth != month) || (time.wDay != day) )
    {
        CloseHandle( hFile );
        NewLogFile();
        //
        hFile = CreateFile( arrFullFilename, GENERIC_READ|GENERIC_WRITE,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, 0 );
        if ( hFile == INVALID_HANDLE_VALUE )
            LastError();
        //write_header();
    }
    //
    //
    unsigned long numOfByte;
    SetFilePointer( hFile, 0, NULL, FILE_END );
    //WriteFile( hFile, CrLf, lstrlen(CrLf), &numOfByte, 0 );
    WriteFile( hFile, timeStamp, lstrlen(timeStamp), &numOfByte, 0 );
    //
    WriteFile( hFile, msg, lstrlen(msg), &numOfByte, 0 );
    //
	WriteFile( hFile, CrLf, lstrlen(CrLf), &numOfByte, 0 );
	//
#ifdef SIZE_LIMIT
    Shrink();
#endif
    //
    LeaveCriticalSection( &hLock );
}
//---------------------------------------------------------------------------
/*
	[CR][LF]message[CR][LF]
*/
void TLogger::WriteBuffer( char *msg )
{
    EnterCriticalSection( &hLock );
    //
    GetLocalTime( &time );
    static char timeStamp[30] = {0};
    wsprintf( timeStamp, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] \0",
              time.wYear, time.wMonth, time.wDay,
              time.wHour, time.wMinute, time.wSecond, time.wMilliseconds );
    if ( (time.wYear != year) || (time.wMonth != month) || (time.wDay != day) )
    {
        CloseHandle( hFile );
        NewLogFile();
        //
        hFile = CreateFile( arrFullFilename, GENERIC_READ|GENERIC_WRITE,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, 0 );
        if ( hFile == INVALID_HANDLE_VALUE )
            LastError();
    }
    //
    //
    unsigned long numOfByte;
    SetFilePointer( hFile, 0, NULL, FILE_END );
    //WriteFile( hFile, CrLf, lstrlen(CrLf), &numOfByte, 0 );
    WriteFile( hFile, timeStamp, lstrlen(timeStamp), &numOfByte, 0 );
    WriteFile( hFile, msg, lstrlen(msg), &numOfByte, 0 );
    WriteFile( hFile, CrLf, lstrlen(CrLf), &numOfByte, 0 );
    //
#ifdef SIZE_LIMIT
    Shrink();
#endif
    //
    LeaveCriticalSection( &hLock );
}
//---------------------------------------------------------------------------
unsigned int TLogger::RenderHexData( unsigned char *buf, const unsigned char *data, unsigned int size )
{
    const unsigned char __HexTable__[] = "0123456789ABCDEF";
    const int MAX_POS = 15;
    unsigned char cur;
    unsigned char *pHex = &buf[0];
    unsigned char *pAscii = NULL;
    unsigned int line_pos = 0;
    //
    for( unsigned int i=0; i<size; i++, pAscii++ )
    {
        line_pos = (i & 0x0f);
        if ( !line_pos )
        {
            if (i) //count to 16th byte
            {	//line break with CR,LF
                pAscii[0] = '\r';	//carriage return (0x0d)
                pAscii[1] = '\n';	//new line (0x0a)
                pHex = pAscii + 2;
            }
			//
            pHex[0] = __HexTable__[(i >> 12) & 0x0F];
            pHex[1] = __HexTable__[(i >> 8)  & 0x0F];
            pHex[2] = __HexTable__[(i >> 4)  & 0x0F];
            pHex[3] = __HexTable__[i & 0x0F];
            pHex[4] = ':';
            pHex[5] = ' ';
            pHex += 6;
            pAscii = pHex + 48;
            pAscii[0] = ';';
            pAscii += 1;
        }
		//
        cur = data[i];
        pHex[0] = __HexTable__[(cur >> 4) & 0x0F];
        pHex[1] = __HexTable__[cur & 0x0F];
        pHex[2] = ' ';
        pHex += 3;
		*pAscii = ( cur >= 0x20 && cur <= 0x7f ) ? cur : '.';
    }
    //
    if ( pAscii == NULL )
        return 0;
    //
    int rest = MAX_POS - line_pos;
    while( rest-- )
    {
        pHex[0] = ' ';
        pHex[1] = ' ';
        pHex[2] = ' ';
        pHex += 3;
        pAscii[0] = ' ';
        pAscii += 1;
    }
	//end of text file
	pAscii[0] = '\r';
	pAscii[1] = '\n';
    pAscii += 2;

    return pAscii - buf;
}
//---------------------------------------------------------------------------
void TLogger::DumpHex1( unsigned char *data, unsigned int size )
{
	//if ( size > 1024 )
	//	throw;

    EnterCriticalSection( &hLock );
    //
    unsigned long num_of_byte;
    // Write time stamp and formatted string to log-file
    SetFilePointer( hFile, 0, NULL, FILE_END );
	//
	unsigned char *buff = new unsigned char[DUMP_BUFF_SIZE];
	ZeroMemory( buff, DUMP_BUFF_SIZE );
	unsigned int len = RenderHexData( buff, data, size );
    WriteFile( hFile, buff, len, &num_of_byte, 0 );
    //Clipboard()->SetTextBuf( buff );
	delete []buff;
    //
#ifdef SIZE_LIMIT
    Shrink();
#endif
    //
    LeaveCriticalSection( &hLock );
}
//---------------------------------------------------------------------------
void TLogger::DumpHex2( unsigned char *data, unsigned char *buff, unsigned int size )
{
    EnterCriticalSection( &hLock );
    //
    unsigned long num_of_byte;
    // Write time stamp and formatted string to log-file
    SetFilePointer( hFile, 0, NULL, FILE_END );
	//
	ZeroMemory( buff, DUMP_BUFF_SIZE );
	unsigned int len = RenderHexData( buff, data, size );
    WriteFile( hFile, buff, len, &num_of_byte, 0 );
    //
    //Clipboard()->SetTextBuf( buff );
    //
#ifdef SIZE_LIMIT
    Shrink();
#endif
    //
    LeaveCriticalSection( &hLock );
}
//---------------------------------------------------------------------------
void TLogger::Printf( const char *fmt, ... )
{
    EnterCriticalSection( &hLock );
    //
    // Format string
    char buffer[DUMP_BUFF_SIZE] = {0};
    va_list arglist;
    va_start( arglist, fmt );
    wvsprintf( buffer, fmt, arglist );
    va_end( arglist );
    //
    unsigned long num_of_byte;
    // Write time stamp and formatted string to log-file
    SetFilePointer( hFile, 0, NULL, FILE_END );
    WriteFile( hFile, buffer, lstrlen(buffer), &num_of_byte, 0 );
    //WriteFile( hFile, CrLf, lstrlen(CrLf), &num_of_byte, 0 );
    //
#ifdef SIZE_LIMIT
    Shrink();
#endif
    //
    LeaveCriticalSection( &hLock );
}
//---------------------------------------------------------------------------
