#include "stdafx.h"
#include "Comparer.h"
#include "ComparerDoc.h"
#include "ComparerViewC.h"
#include "ComparerViewL.h"
#include "ComparerPane.h"

IMPLEMENT_DYNCREATE(CComparerViewL, CView)

BEGIN_MESSAGE_MAP(CComparerViewL, CComparerViewC)
	ON_WM_CREATE()
END_MESSAGE_MAP()

int CComparerViewL::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CComparerViewC::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Add your specialized creation code here
	CComparerDoc *pDoc = GetDocument();
	ComparerPane *pane = &pDoc->mPane[CComparerDoc::IMG_VIEW_L];

	pane->pView = this;

	return 0;
}

CComparerViewC *CComparerViewL::GetOppoView(CComparerDoc *pDoc)
{
	ComparerPane *pane = &pDoc->mPane[CComparerDoc::IMG_VIEW_R];

	return pane->pView;
}

void CComparerViewL::ProcessDocument(CComparerDoc *pDoc)
{
	pDoc->ProcessDocument(pDoc->mPane + CComparerDoc::IMG_VIEW_L);
}

ComparerPane *CComparerViewL::GetPane(CComparerDoc* pDoc) const
{
	ComparerPane *pane = &pDoc->mPane[CComparerDoc::IMG_VIEW_L];

	return pane;
}
