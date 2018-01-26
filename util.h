//---------------------------------------------------------------------------

#ifndef utilH
#define utilH

//---------------------------------------------------------------------------
char* __fastcall ticks_string( void );
char* __fastcall TimeStamp( SYSTEMTIME time );
char *GetPeerSocketIp( SOCKET s );
int __fastcall GetPeerSocketPort( SOCKET s );
String __fastcall utf8_to_ansi( const char *buf );
String __fastcall ansi_to_utf8( const char *buf );
void __fastcall Shuffle( char *nums, int a, int b );
String __fastcall LongToString( long n );
void __fastcall EnableDebugPriv( void );
unsigned long __fastcall GetProcessId( char *name );
void __fastcall ExtractStringToList(TStringList *list, String fields);
void __fastcall ExtractStringToList1(TStringList *list, String fields);
void __fastcall GetVersionInfo(int &major, int &minor, int &tiny, int &build);
bool __fastcall valid_utf8_file(const char* file_name);
bool __fastcall start_with_bom( void );
//void __fastcall fix_utf8_string(std::string& str);
char* __fastcall debug_info( unsigned long pid, unsigned long line );

#endif
