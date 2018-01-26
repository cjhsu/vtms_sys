
#ifndef LoggerH
#define LoggerH
//---------------------------------------------------------------------------
#define STRINGIFY(x)    #x
#define TOSTRING(x)     STRINGIFY(x)

#define LINE            TOSTRING(__LINE__)
#define FUNC            (__FUNC__)

#define LOG_MSG         Logger.Message
#define LOG_LASTERROR	Logger.LastError
#define LOG_DUMP1		Logger.DumpHex1
#define LOG_DUMP2		Logger.DumpHex2
#define LOG_DATA        Logger.WriteBuffer
#define LOG_PRINTF		Logger.Printf
//---------------------------------------------------------------------------
const char LINE_CR = 0x0D;
const char LINE_LF = 0x0A;
const char CrLf[4] = { LINE_CR, LINE_LF, 0 };
const int MAX_CHAR_PER_LINE = 256;
const int CHARS_PER_LINE = 73;  //include "\r\n"
const int DUMP_BUFF_SIZE = (CHARS_PER_LINE * (65536/16+1)); //65536 bytes

//#define SIZE_LIMIT
//---------------------------------------------------------------------------
class TLogger
{
private:
	CRITICAL_SECTION hLock;		// lock for thread-safe access
    HANDLE hMutex;
    HANDLE hFile;
    char arrPath[_MAX_PATH+1];
    char arrFilename[_MAX_FNAME+1];
    char arrFullFilename[_MAX_FNAME+1];
    unsigned short year, month, day;
	char filename_prefix[_MAX_FNAME+1];
    SYSTEMTIME time;
	//
    void CloseLog();
    void NewLogFile( void );
    //void WriteHeader( void );
    void Shrink( void );
	unsigned int RenderHexData( unsigned char *buff, const unsigned char *data, unsigned int size );

public:
	TLogger( char *path, char *prefix );
	virtual ~TLogger();

    void LastError( void );
	void Message( char *msg );
	void Debug( char *msg, unsigned long pid, unsigned long line );
	void DumpHex1( unsigned char *buffer, unsigned int size );
    void DumpHex2( unsigned char *data, unsigned char *buff, unsigned int size );    
	void WriteBuffer( char *msg );
	void Printf( const char *fmt, ... );
};
//---------------------------------------------------------------------------
// Header Shield
//
#ifdef __LOGGER_CPP__
#define AUTOEXT
#else
#define AUTOEXT extern
#endif

AUTOEXT TLogger Logger( ".\\log\\", NULL );

#undef AUTOEXT
//---------------------------------------------------------------------------
#endif
