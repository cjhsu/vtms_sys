//---------------------------------------------------------------------------
#ifndef GbltypeH
#define GbltypeH

const unsigned short MAXLEN_CONTEXT_BUFF = 8192;
const unsigned short MAXLEN_SOCKIPPORT = 31; //1234.1234.1234.1234:65535\0
//---------------------------------------------------------------------------
typedef enum _IO_OPERATION
{
        CLIENT_IO_READ = 0,
        CLIENT_IO_WRITE
} IO_OPERATION;

// data to be associated for every I/O operation on a socket
typedef struct tagPER_IO_CONTEXT
{
        WSAOVERLAPPED Overlapped;

        IO_OPERATION IOOperation;
        //
        WSABUF wsabuf;
        char rbuf[MAXLEN_CONTEXT_BUFF+1];
        char sbuf[MAXLEN_CONTEXT_BUFF+1];

        int nTotalBytes;    //總共送出的 bytes 數目

        int nSentBytes;     //已送出的 bytes 計數

} PER_IO_CONTEXT, *PPER_IO_CONTEXT;
//------------------------------------------------------------------------------
// For AcceptEx, the IOCP key is the PER_SOCKET_CONTEXT for the listening socket,
// so we need to another field SocketAccept in PER_IO_CONTEXT. When the outstanding
// AcceptEx completes, this field is our connection socket handle.

// data to be associated with every socket added to the IOCP

using namespace std;
typedef struct tagPER_SOCKET_CONTEXT
{
        SOCKET Socket;
        char SockIp[MAXLEN_SOCKIPPORT];
        //[0] for READ
        //[1] for WRITE
        PER_IO_CONTEXT IOContext[2];
        //
	    DWORD dwOutstandingOverlappedIO;
        //
        DWORD Ticks;
        vector<char> *Buff;
        //
        String Name;
        String Machine;
        String Checkpoint;
        String Station;
        TStringList *List;
        TStringList *nvr_ip; //紀錄與 data cloner ip 關聯的 NVR ip address
        //
        Json::FastWriter fast_writer;
        Json::Value root;

} PER_SOCKET_CONTEXT, *PPER_SOCKET_CONTEXT;
//---------------------------------------------------------------------------
#endif
 