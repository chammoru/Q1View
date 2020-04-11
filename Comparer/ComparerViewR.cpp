#include "stdafx.h"
#include "Comparer.h"
#include "ComparerDoc.h"
#include "ComparerViewC.h"
#include "ComparerViewR.h"
#include "ComparerPane.h"

IMPLEMENT_DYNCREATE(CComparerViewR, CView)

BEGIN_MESSAGE_MAP(CComparerViewR, CComparerViewC)
	ON_WM_CREATE()
END_MESSAGE_MAP()

int CComparerViewR::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CComparerViewC::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Add your specialized creation code here
	CComparerDoc *pDoc = GetDocument();
	ComparerPane *pane = &pDoc->mPane[CComparerDoc::IMG_VIEW_R];

	pane->pView = this;

	return 0;
}

CComparerViewC *CComparerViewR::GetOppoView(CComparerDoc *pDoc)
{
	ComparerPane *pane = &pDoc->mPane[CComparerDoc::IMG_VIEW_L];

	return pane->pView;
}

void CComparerViewR::ProcessDocument(CComparerDoc *pDoc)
{
	pDoc->ProcessDocument(pDoc->mPane + CComparerDoc::IMG_VIEW_R);
}

ComparerPane *CComparerViewR::GetPane(CComparerDoc* pDoc) const
{
	ComparerPane *pane = &pDoc->mPane[CComparerDoc::IMG_VIEW_R];

	return pane;
}
