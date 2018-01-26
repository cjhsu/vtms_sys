//---------------------------------------------------------------------------
#ifndef TypedListH
#define TypedListH

#include <classes.hpp>
//---------------------------------------------------------------------------
template <class T>
class TTypedList : public TList
{
private:
    bool bAutoDelete;
protected:
    T* __fastcall Get( int Index )
    {
        return (T*) TList::Get( Index );
    }

    void __fastcall Put( int Index, T* Item )
    {
        TList::Put( Index, Item );
    }

public:
    __fastcall TTypedList(bool bFreeObjects = false):TList(), bAutoDelete(bFreeObjects)
    {
    }

    // Note: No destructor needed. TList::Destroy calls Clear,
    //       and Clear is virtual, so our Clear runs.

    int __fastcall Add( T* Item )
    {
        return TList::Add( Item );
    }

    void __fastcall Delete( int Index )
    {
        if ( bAutoDelete )
        {
            T *temp = reinterpret_cast<T*>(Get( Index ));
            delete temp;
            //Get( Index ) = (void*)NULL;
        }
        TList::Delete( Index );
    }

    void __fastcall Clear( void )
    {
        if( bAutoDelete )
        {
            T *temp = NULL;
            for (int j=0; j<Count; j++)
            {
                temp = reinterpret_cast<T*>(Items[j]);
                delete temp;
                Items[j] = NULL;
            }
        }
        TList::Clear();
    }

    T* __fastcall First( void )
    {
        return (T*)TList::First();
    }

    int __fastcall IndexOf( T* Item )
    {
        return TList::IndexOf( Item );
    }

    void __fastcall Insert( int Index, T* Item )
    {
        TList::Insert( Index, Item );
    }

    T* __fastcall Last( void )
    {
        return (T*) TList::Last();
    }

    int __fastcall Remove( T* Item )
    {
        int nIndex = TList::Remove( Item );
        // Should I delete a pointer that is being passed to me.
        // If bAutoDelete is true, then assume that we are always
        // responsible for deleting a pointer that is added to the
        // list. If the item was found, then delete the pointer.
        if ( bAutoDelete && (nIndex != -1) )
        {
            T *temp = reinterpret_cast<T*>(Item);
            delete temp;
            Item = NULL;
        }
        return nIndex;
    }

    __property T* Items[int Index] = {read=Get, write=Put};
};

#endif
