//---------------------------------------------------------------------------
#include <vcl.h>
#include <memory>
#include <StrUtils.hpp>
#pragma hdrstop

#include "VtmsThread.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

#define CRLF  "\x0d\x0a\0"
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
int TVtmsThread::Count = 0;
//---------------------------------------------------------------------------
__fastcall TVtmsThread::TVtmsThread( void *obj, String host, int port, bool auto_free )
		: TThread( true ) //create but don't run
        ,Host(host), Port(port)
{
    Count ++;
	FreeOnTerminate = true;
    Priority = tpLower;

    //FTerminateEvent = new TEvent( NULL, true, false, "FTerminateEvent");
}
//---------------------------------------------------------------------------
__fastcall TVtmsThread::~TVtmsThread( void )
{
    //delete FTerminateEvent;

    if ( Count )
        Count--;
}
//---------------------------------------------------------------------------
void __fastcall TVtmsThread::PostData( String value )
{
    std::auto_ptr<TStringList>response(new TStringList);
	std::auto_ptr<TStringStream>params(new TStringStream(""));
	try
    {
    	String paramEncoded = TIdURI::ParamsEncode(__classid(TIdURI), UTF8Encode(value));
    	String paramFixed = StringReplace(paramEncoded, "\"", "%22",
		    TReplaceFlags() << rfReplaceAll); // Indy9 ¾A¥Î
	    params->WriteString(paramFixed);
		//
		try
        {
			url = Format("http://%s:%d/waybill.json", OPENARRAY(TVarRec, (Host, Port)));
            //
            if (FBeforeHttpPost)
                FBeforeHttpPost( this, url, value.c_str() );
            //
            httpClient->Host = Host;
            httpClient->Port = Port;            
			String response_text = httpClient->Post(url, &(*params));
            response->Text = UTF8Decode(TIdURI::URLDecode(__classid(TIdURI), response_text));
            //
            if ( FOnHttpResponse )
                FOnHttpResponse( this, httpClient->ResponseCode, response->Text );
        }
		catch (Exception &e)
        {
            if ( FOnHttpException )
                FOnHttpException( this, AnsiReplaceStr(e.Message, String(CRLF), ",") );
		}
        //
	}
	__finally
    {
	}
}
//---------------------------------------------------------------------------
void __fastcall TVtmsThread::OnStatus(TObject *Sender, const TIdStatus Status, const String StatusText)
{
    if (FOnHttpStatus)
        FOnHttpStatus( this, StatusText );
}
//---------------------------------------------------------------------------
void __fastcall TVtmsThread::Execute( void )
{
    antiFreeze = new TIdAntiFreeze(NULL);
	antiFreeze->Active = true;
	antiFreeze->ApplicationHasPriority = true;
	antiFreeze->IdleTimeOut = 30;
	antiFreeze->OnlyWhenIdle = true;
    //
	httpClient = new TIdHTTP(NULL);
	httpClient->HandleRedirects = true; // 302 error
	httpClient->Request->ContentType = "application/x-www-form-urlencoded";
	httpClient->AllowCookies = false;
	httpClient->ReadTimeout = 60000;
	httpClient->ConnectTimeout = 60000; // ??? fix EIdReadTimeout ???
    httpClient->OnStatus = this->OnStatus;
    //
    try
    {
        PostData( post_body );
    }
    __finally
    {
        httpClient->Disconnect();
        delete antiFreeze;
        delete httpClient;
    }
}
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
