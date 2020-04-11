#pragma once

class CComparerViewL : public CComparerViewC
{
	DECLARE_DYNCREATE(CComparerViewL)

	virtual CComparerViewC *GetOppoView(CComparerDoc *pDoc);
	virtual ComparerPane *GetPane(CComparerDoc* pDoc) const;
	virtual void ProcessDocument(CComparerDoc *pDoc);

public:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};
