/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		        TT Get Level Score
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "StdAfx.h"

#include "ErrorProtocol.h"
#include "InterfaceManager.h"

CKObjectDeclaration *FillBehaviorGetLevelScoreDecl();
CKERROR CreateGetLevelScoreProto(CKBehaviorPrototype **);
int GetLevelScore(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorGetLevelScoreDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("TT Get Level Score");
    od->SetDescription("Gets level score from manager");
    od->SetCategory("TT InterfaceManager/LevelInfo Behaviors");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x7FE65A65, 0x490A49B5));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateGetLevelScoreProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateGetLevelScoreProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("TT Get Level Score");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");

    proto->DeclareOutput("Out");

    proto->DeclareOutParameter("Level Score", CKPGUID_INT);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(GetLevelScore);

    *pproto = proto;
    return CK_OK;
}

int GetLevelScore(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *context = behcontext.Context;

    CTTInterfaceManager *man = CTTInterfaceManager::GetManager(context);
    if (!man)
    {
        TT_ERROR("GetLevelScore.cpp", "int GetLevelScore(...)", " im == NULL");
        return CKBR_OK;
    }

    CGameInfo *gameInfo = man->GetGameInfo();
    if (!gameInfo)
    {
        ::PostMessageA((HWND)context->GetRenderManager()->GetRenderContext(man->GetDriverIndex())->GetWindowHandle(), TT_MSG_NO_GAMEINFO, 0x0E, 0);
        TT_ERROR("GetLevelScore.cpp", "int GetLevelScore(...)", " gameInfo not exists");
    }

    beh->SetOutputParameterValue(0, &gameInfo->levelScore);

    beh->ActivateOutput(0);
    return CKBR_OK;
}