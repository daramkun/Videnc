#pragma once

using namespace System;

namespace Daramee
{
	namespace Videnc
	{
		private class ManagedIStream : public IStream
		{
		public:
			ManagedIStream ( System::IO::Stream ^ baseStream )
				: refCount ( 1 )
			{
				this->baseStream = baseStream;
				tempBytes = gcnew array<byte, 1> ( 4194304 );
			}

		public:
			virtual HRESULT STDMETHODCALLTYPE QueryInterface ( REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject )
			{
				if ( riid == __uuidof ( IStream ) ) { *ppvObject = this; AddRef (); return S_OK; }
				else { *ppvObject = NULL; return E_NOINTERFACE; }
			}
			virtual ULONG STDMETHODCALLTYPE AddRef ( void ) { return InterlockedIncrement ( &refCount ); }
			virtual ULONG STDMETHODCALLTYPE Release ( void )
			{
				ULONG temp = InterlockedDecrement ( &refCount );
				if ( temp == 0 ) delete this;
				return temp;
			}

		public:
			// IStream을(를) 통해 상속됨
			virtual HRESULT STDMETHODCALLTYPE Read ( _Out_writes_bytes_to_ ( cb, *pcbRead ) void *pv, _In_  ULONG cb, _Out_opt_ ULONG *pcbRead )
			{
				if ( !baseStream->CanRead ) return E_ACCESSDENIED;
				if ( ( ( int ) cb ) >= tempBytes->Length ) tempBytes = gcnew array<byte, 1> ( cb + 1 );
				int bytesRead = baseStream->Read ( tempBytes, 0, cb );
				System::Runtime::InteropServices::Marshal::Copy ( tempBytes, 0, System::IntPtr ( pv ), bytesRead );
				if ( pcbRead != nullptr ) *pcbRead = bytesRead;
				return S_OK;
			}
			virtual HRESULT STDMETHODCALLTYPE Write ( _In_reads_bytes_ ( cb )  const void *pv, _In_  ULONG cb, _Out_opt_  ULONG *pcbWritten )
			{
				if ( !baseStream->CanWrite ) return E_ACCESSDENIED;
				if ( ( ( int ) cb ) >= tempBytes->Length ) tempBytes = gcnew array<byte, 1> ( cb + 1 );
				System::Runtime::InteropServices::Marshal::Copy ( System::IntPtr ( ( void * ) pv ), tempBytes, 0, cb );
				baseStream->Write ( tempBytes, 0, cb );
				if ( pcbWritten != nullptr ) *pcbWritten = cb;
				return S_OK;
			}
			virtual HRESULT STDMETHODCALLTYPE Seek ( LARGE_INTEGER dlibMove, DWORD dwOrigin, _Out_opt_  ULARGE_INTEGER *plibNewPosition )
			{
				if ( !baseStream->CanSeek ) return E_ACCESSDENIED;
				System::IO::SeekOrigin seekOrigin;

				switch ( dwOrigin )
				{
				case 0: seekOrigin = System::IO::SeekOrigin::Begin; break;
				case 1: seekOrigin = System::IO::SeekOrigin::Current; break;
				case 2: seekOrigin = System::IO::SeekOrigin::End; break;
				default: throw gcnew ArgumentOutOfRangeException ( "dwOrigin" );
				}

				long long position = baseStream->Seek ( dlibMove.QuadPart, seekOrigin );
				if ( plibNewPosition != nullptr )
					plibNewPosition->QuadPart = position;
				return S_OK;
			}
			virtual HRESULT STDMETHODCALLTYPE SetSize ( ULARGE_INTEGER libNewSize )
			{
				baseStream->SetLength ( libNewSize.QuadPart );
				return S_OK;
			}
			virtual HRESULT STDMETHODCALLTYPE CopyTo ( _In_  IStream *pstm, ULARGE_INTEGER cb, _Out_opt_  ULARGE_INTEGER *pcbRead, _Out_opt_  ULARGE_INTEGER *pcbWritten )
			{
				return E_NOTIMPL;
			}
			virtual HRESULT STDMETHODCALLTYPE Commit ( DWORD grfCommitFlags ) { return E_NOTIMPL; }
			virtual HRESULT STDMETHODCALLTYPE Revert ( void ) { return E_NOTIMPL; }
			virtual HRESULT STDMETHODCALLTYPE LockRegion ( ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType ) { return E_NOTIMPL; }
			virtual HRESULT STDMETHODCALLTYPE UnlockRegion ( ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType ) { return E_NOTIMPL; }
			virtual HRESULT STDMETHODCALLTYPE Stat ( __RPC__out STATSTG *pstatstg, DWORD grfStatFlag )
			{
				memset ( pstatstg, 0, sizeof ( STATSTG ) );
				pstatstg->type = STGTY_STREAM;
				pstatstg->cbSize.QuadPart = baseStream->Length;

				if ( baseStream->CanRead && baseStream->CanWrite )
					pstatstg->grfMode |= STGM_READWRITE;
				else if ( baseStream->CanRead )
					pstatstg->grfMode |= STGM_READ;
				else if ( baseStream->CanWrite )
					pstatstg->grfMode |= STGM_WRITE;
				else throw gcnew System::IO::IOException ();
				return S_OK;
			}
			virtual HRESULT STDMETHODCALLTYPE Clone ( __RPC__deref_out_opt IStream **ppstm ) { return E_NOTIMPL; }

		private:
			ULONG refCount;
			gcroot<System::IO::Stream^> baseStream;
			gcroot<array<byte, 1>^> tempBytes;
		};
	}
}