#include "Stream.hpp"

#include <cstdio>
#include <Shlwapi.h>

#pragma comment ( lib, "Shlwapi.lib" )

IStream * CreateStreamFromFilename ( LPCTSTR filename, bool newFile )
{
	IStream * temp;
	DWORD flag = STGM_READWRITE;
	if ( newFile )
		flag |= STGM_CREATE;
	SHCreateStreamOnFile ( filename, flag, &temp );
	return temp;
}

class StandardOutputStream : public IStream
{
public:
	StandardOutputStream () : refCount ( 1 ), stdHandle ( GetStdHandle ( STD_OUTPUT_HANDLE ) ) { }
	virtual ~StandardOutputStream () { CloseHandle ( stdHandle ); }

public:
	virtual HRESULT STDMETHODCALLTYPE QueryInterface ( REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject )
	{
		if ( riid == __uuidof ( IStream ) ) { *ppvObject = this; AddRef (); return S_OK; }
		else { *ppvObject = NULL; return E_NOINTERFACE; }
	}

	virtual ULONG STDMETHODCALLTYPE AddRef ( void ) { return InterlockedIncrement ( &refCount ); }
	virtual ULONG STDMETHODCALLTYPE Release ( void ) { ULONG newCount = InterlockedDecrement ( &refCount ); if ( newCount == 0 ) delete this; return newCount; }

public:
	virtual HRESULT STDMETHODCALLTYPE Read ( _Out_writes_bytes_to_ ( cb, *pcbRead ) void *pv, _In_  ULONG cb, _Out_opt_ ULONG *pcbRead ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Write ( _In_reads_bytes_ ( cb )  const void *pv, _In_  ULONG cb, _Out_opt_  ULONG *pcbWritten ) { return WriteConsole ( stdHandle, pv, cb, pcbWritten, nullptr ) ? S_OK : E_FAIL; }

public:
	virtual HRESULT STDMETHODCALLTYPE Seek ( LARGE_INTEGER dlibMove, DWORD dwOrigin, _Out_opt_  ULARGE_INTEGER *plibNewPosition ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE SetSize ( ULARGE_INTEGER libNewSize ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE CopyTo ( _In_  IStream *pstm, ULARGE_INTEGER cb, _Out_opt_  ULARGE_INTEGER *pcbRead, _Out_opt_  ULARGE_INTEGER *pcbWritten ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Commit ( DWORD grfCommitFlags ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Revert ( void ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE LockRegion ( ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE UnlockRegion ( ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Stat ( __RPC__out STATSTG *pstatstg, DWORD grfStatFlag ) {
		pstatstg->type = STGTY_STREAM;
		pstatstg->cbSize.QuadPart = 0;
		pstatstg->grfMode = STGM_WRITE;
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE Clone ( __RPC__deref_out_opt IStream **ppstm ) { return E_NOTIMPL; }

private:
	ULONG refCount;
	HANDLE stdHandle;
};

IStream * CreateStreamFromStandardOutputHandle ()
{
	return new StandardOutputStream ();
}
