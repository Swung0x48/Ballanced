/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		           TT Write Array
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "StdAfx.h"

#include <stdio.h>

#include "ErrorProtocol.h"
#include "InterfaceManager.h"

CKObjectDeclaration *FillBehaviorWriteArrayDecl();
CKERROR CreateWriteArrayProto(CKBehaviorPrototype **);
int WriteArray(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorWriteArrayDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("TT Write Array");
    od->SetDescription("Writes arrays to manager");
    od->SetCategory("TT InterfaceManager/General");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x7414AF4, 0x505E52EE));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateWriteArrayProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateWriteArrayProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("TT Write Array");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");

    proto->DeclareOutput("Out");

    proto->DeclareInParameter("Name of Array", CKPGUID_DATAARRAY);
    proto->DeclareInParameter("Filename of CMO", CKPGUID_STRING);
    proto->DeclareInParameter("Show message: Wrote Array", CKPGUID_BOOL);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(WriteArray);

    *pproto = proto;
    return CK_OK;
}

int WriteArray(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *context = behcontext.Context;

    beh->ActivateInput(0, FALSE);

    CTTInterfaceManager *man = CTTInterfaceManager::GetManager(context);
    if (!man)
    {
        ::PostMessageA((HWND)context->GetRenderManager()->GetRenderContext(0)->GetWindowHandle(), TT_MSG_NO_GAMEINFO, 0x1E, 1);
        TT_ERROR("WriteArray.cpp", "int WriteArray(...)", " gameInfo not exists");
    }

    CKDataArray *array = (CKDataArray *)beh->GetInputParameterObject(0);
    CKSTRING cmo = (CKSTRING)beh->GetInputParameterReadDataPtr(1);

    CNemoArrayList *nemoArraylist = man->GetNemoArrayList();
    CNemoArray *nemoArray = nemoArraylist->Search(cmo, array);
    if (nemoArray)
    {
        nemoArray->Write(array);

        BOOL bShowMessage = FALSE;
        beh->GetInputParameterValue(2, &bShowMessage);
        if (bShowMessage)
        {
            char *msg = new char[256];
            sprintf(msg, " '%s'  from file '%s' written to manager", array->GetName(), cmo);
            ::MessageBoxA((HWND)context->GetRenderManager()->GetRenderContext(man->GetDriverIndex())->GetWindowHandle(), msg, "message...", MB_OK);
            delete[] msg;
        }
    }

    beh->ActivateOutput(0);
    return CKBR_OK;
}