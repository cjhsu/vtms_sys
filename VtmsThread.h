//---------------------------------------------------------------------------

#ifndef VtmsThreadH
#define VtmsThreadH

#include <SyncObjs.hpp>
#include <IdHTTP.hpp>
#include <IdAntiFreeze.hpp>

typedef void __fastcall (*THttpStatusEvent)( TObject *Sender, String status_text );
typedef void __fastcall (*THttpExceptionEvent)( TObject *Sender, String message );
typedef void __fastcall (*THttpResponseEvent)( TObject *Sender, int code, String response );
typedef void __fastcall (*THttpBeforePostEvent)( TObject *Sender, String url, String body );
typedef void __fastcall (*TThreadTerminateEvent)( TObject *Sender );
//---------------------------------------------------------------------------
class TVtmsThread : public TThread
{
 private:
        THttpExceptionEvent FOnHttpException;
        THttpResponseEvent FOnHttpResponse;
        THttpStatusEvent FOnHttpStatus;
        THttpBeforePostEvent FBeforeHttpPost;
        TEvent *FTerminateEvent;
        TThreadTerminateEvent *FOnTerminateEvent;

        TIdAntiFreeze *antiFreeze;
        TIdHTTP *httpClient;

        String post_body;
        String Host;
        int Port;
     	System::String url; // Protocol + Host + Port + ScriptName
        void __fastcall PostData( String Value );

 protected:
        virtual void __fastcall Execute( void );
        void __fastcall OnStatus(TObject *Sender, const TIdStatus Status, const String StatusText);

 public:
        static int Count;

        __fastcall TVtmsThread( void *obj, String host, int port, bool auto_free=true );
        virtual __fastcall ~TVtmsThread();

        __property String PostBody = { write=post_body };
        __property String Url = { read=url };

        __property THttpStatusEvent OnHttpStatus = { write=FOnHttpStatus };
        __property THttpResponseEvent OnHttpResponse = { write=FOnHttpResponse };
        __property THttpExceptionEvent OnHttpException = { write=FOnHttpException };
        __property THttpBeforePostEvent BeforeHttpPost = { write=FBeforeHttpPost };
        //__property TThreadTerminateEvent OnTerminate = { write=FOnTerminateEvent };
};
//---------------------------------------------------------------------------
#endif
