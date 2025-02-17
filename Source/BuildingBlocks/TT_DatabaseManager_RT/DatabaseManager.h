#ifndef BUILDINGBLOCKS_DATABASEMANAGER_H
#define BUILDINGBLOCKS_DATABASEMANAGER_H

#include "CKBaseManager.h"

#define TT_DATABASE_MANAGER_GUID CKGUID(0x4DB6188E, 0x287E1410)

class CTTDatabaseManager : public CKBaseManager
{
public:
	CTTDatabaseManager(CKContext *ctx);
	~CTTDatabaseManager();

	CKERROR OnCKInit() { return CK_OK; }
	CKERROR OnCKEnd() { return CK_OK; }
	CKERROR OnCKPlay() { return CK_OK; }
	CKERROR OnCKReset()
	{
		Clear();
		m_Filename = NULL;
		m_Crypted = TRUE;
		return CK_OK;
	}
	CKERROR PreClearAll() { return CK_OK; }
	CKERROR PostClearAll() { return CK_OK; }
	CKERROR PostProcess() { return CK_OK; }
	CKDWORD GetValidFunctionsMask()
	{
		return CKMANAGER_FUNC_PostProcess |
			   CKMANAGER_FUNC_PreClearAll |
			   CKMANAGER_FUNC_PostClearAll |
			   CKMANAGER_FUNC_OnCKInit |
			   CKMANAGER_FUNC_OnCKEnd |
			   CKMANAGER_FUNC_OnCKPlay |
			   CKMANAGER_FUNC_OnCKReset;
	}

	int Register(CKSTRING arrayName);

	int Clear();

	int Load(CKContext *context, bool autoRegister, CKSTRING arrayName);

	int Save(CKContext *context);

	bool SetProperty(CKSTRING filename, BOOL crypted);

	static CTTDatabaseManager *GetManager(CKContext *context)
	{
		return (CTTDatabaseManager *)context->GetManagerByGuid(TT_DATABASE_MANAGER_GUID);
	}

protected:
	CKContext *m_Context;
	bool field_2C;
	XArray<CKSTRING> m_ArrayNames;
	CKSTRING m_Filename;
	BOOL m_Crypted;
};

#endif // BUILDINGBLOCKS_DATABASEMANAGER_H