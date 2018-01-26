//---------------------------------------------------------------------------
#include <vcl>
#include <StrUtils.hpp>
#pragma hdrstop

//#include "Logger.h"
#include "IdHTTP.hpp"
#include "QCV.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
__fastcall TQCV::TQCV()
        : TThread( true ), FOnRequest( NULL), FOnResponse( NULL ) //create but don't run
{
    FreeOnTerminate = true;
    Priority = tpNormal;
    resp_list = new TStringList;

    run = false;
    Resume();
}
//---------------------------------------------------------------------------
__fastcall TQCV::~TQCV( void )
{
    delete resp_list;
}
//---------------------------------------------------------------------------
void __fastcall TQCV::sync_event( void )
{
    if ( FOnRequest )
        FOnRequest( this );
}
//---------------------------------------------------------------------------
void __fastcall TQCV::Execute( void )
{
    while ( !run );

    //int response_code;
    TMemoryStream *response = new TMemoryStream;

    TIdHTTP *http = new TIdHTTP(NULL);
    http->HandleRedirects = true;
    http->ReadTimeout = 3000;
    http->ConnectTimeout = 3000;
	//http->Request->ContentType = "text/plain";
    http->HTTPOptions = http->HTTPOptions << hoKeepOrigProtocol;
    http->ProtocolVersion = pv1_1;
    http->HTTPOptions = http->HTTPOptions >> hoForceEncodeParams;
    //uri = "http://211.23.156.207:80/snapshot?tag=20171101173110$0000000000$CP1$WSC$1&ch=1&line1=2017-11-01_17:31:10&line2=WSC&line3=CP1&filename=20171101173110$0000000000$CP1$WSC$1.png";
    http->URL->URI = uri;
    String encodeURL = http->URL->URLEncode( __classid(TIdURI), uri );
    encodeURL = AnsiReplaceStr( encodeURL, "_", "%20" );
    //
    encodeURL = AnsiReplaceStr( encodeURL, "\r\n", "%0D%0A" );
    //
    //Synchronize( sync_event );
    //
    try
    {
        response->Clear();
        response->Position = 0;
        response_str = EmptyStr;
        try
        {
            http->Get( encodeURL, response );
            response_code = http->ResponseCode;
        }
        catch( Exception &e )
        {
            String msg = Format( "(%s)%s",
                OPENARRAY( TVarRec,( String(e.ClassName()).c_str(), e.Message.c_str() ) ) );
            puts( msg.c_str() );
        }
    }
    __finally
    {
        //printf( "\n%d: %s\n", response_code, http->Response->ContentType.c_str() );
        //
        if ( CompareText( http->Response->ContentType, "image/png" ) == 0 )
        {
            response->Position = 0;
            response->SaveToFile( "image.png" );
            //
            if ( FOnResponse )
                FOnResponse( this );
        }
        else
            if ( CompareText( http->Response->ContentType, "text/plain" ) == 0 )
            {
                resp_list->Delimiter = '&';
                resp_list->DelimitedText = String( (char*)response->Memory, response->Size );
                response_str = String( (char*)response->Memory, response->Size );
                //
                if ( FOnResponse )
                    FOnResponse( this );
            }

        delete response;
        response = (void*)NULL;
        delete http;
        http = (void*)NULL;

        //this->DoTerminate();
    }
}
//---------------------------------------------------------------------------
