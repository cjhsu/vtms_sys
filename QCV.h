//---------------------------------------------------------------------------

#ifndef QCVH
#define QCVH
//---------------------------------------------------------------------------
//typedef void __fastcall (*TOnRespEvent)( TObject *Sender );
typedef void __fastcall(__closure *TOnReqEvent)( TObject *Sender );
typedef void __fastcall(__closure *TOnRespEvent)( TObject *Sender );

class TQCV : public TThread
{
 private:
    String uri;
    int response_code;
    String response_str;
    String response_content_type;
    void __fastcall Execute( void );
    void __fastcall setURI( String param ){ uri = param; };
    String __fastcall getURI( void ){ return uri; };
    void __fastcall sync_event( void );    

 protected:
    TOnRespEvent FOnResponse;
    TOnReqEvent FOnRequest;    

 public:
    bool run;
    TStringList *resp_list;
    __fastcall TQCV();
    __fastcall ~TQCV();
    __property TOnReqEvent OnRequest = { read=FOnRequest, write=FOnRequest };
    __property TOnRespEvent OnResponse = { read=FOnResponse, write=FOnResponse };
    //__property int ResponseCode = { read=response_code };
    //__property String ResponseContentType = { read=response_content_type };
    __property String ResponseString = { read=response_str };
    __property String URI = { write=setURI, read=getURI };
};


#endif
