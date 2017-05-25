// FileZillaEngine.cpp: Implementierung der Klasse CFileZillaEngine.
//
//////////////////////////////////////////////////////////////////////

#include <filezilla.h>

#include "engineprivate.h"
#include "directorycache.h"
#include "ControlSocket.h"



CFileZillaEngine::CFileZillaEngine(CFileZillaEngineContext& engine_context)
	: impl_(new CFileZillaEnginePrivate(engine_context, *this))
{
}

CFileZillaEngine::~CFileZillaEngine()
{
	delete impl_;
}

int CFileZillaEngine::Init(wxEvtHandler *pEventHandler)
{
	return impl_->Init(pEventHandler);
}

int CFileZillaEngine::Execute(const CCommand &command)
{
	return impl_->Execute(command);
}

std::unique_ptr<CNotification> CFileZillaEngine::GetNextNotification()
{
	return impl_->GetNextNotification();
}

bool CFileZillaEngine::SetAsyncRequestReply(std::unique_ptr<CAsyncRequestNotification> && pNotification)
{
	return impl_->SetAsyncRequestReply(std::move(pNotification));
}

bool CFileZillaEngine::IsPendingAsyncRequestReply(std::unique_ptr<CAsyncRequestNotification> const& pNotification)
{
	return impl_->IsPendingAsyncRequestReply(pNotification);
}

bool CFileZillaEngine::IsActive(enum CFileZillaEngine::_direction direction)
{
	return CFileZillaEnginePrivate::IsActive(direction);
}

CTransferStatus CFileZillaEngine::GetTransferStatus(bool &changed)
{
	return impl_->GetTransferStatus(changed);
}

int CFileZillaEngine::CacheLookup(const CServerPath& path, CDirectoryListing& listing)
{
	return impl_->CacheLookup(path, listing);
}

int CFileZillaEngine::Cancel()
{
	return impl_->Cancel();
}

bool CFileZillaEngine::IsBusy() const
{
	return impl_->IsBusy();
}

bool CFileZillaEngine::IsConnected() const
{
	return impl_->IsConnected();
}
