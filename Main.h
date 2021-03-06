
#ifndef MAIN_H
#define MAIN_H

#include "GblType.h"
//---------------------------------------------------------------------------
bool CreateListenSocket( String listen_ip, int listen_port );

unsigned long WINAPI WorkerThread( LPVOID WorkContext );

PPER_SOCKET_CONTEXT UpdateCompletionPort( SOCKET sd );

void CloseClient( PPER_SOCKET_CONTEXT lpPerSocketContext );

PPER_SOCKET_CONTEXT CtxtAllocate( SOCKET s );

void CtxtListFree( void );

void CtxtListDeleteFrom( PPER_SOCKET_CONTEXT lpPerSocketContext );
//------------------------------------------------------------------------------

#endif


