#include "stdafx.h"
#include "Comparer.h"
#include "ComparerDoc.h"
#include "ComparerView.h"
#include "ComparerViews.h"


#define IMPLEMENT_COMPARER_VIEW(class_name, view_id)            \
	IMPLEMENT_DYNCREATE(class_name, CView)                      \
                                                                \
	BEGIN_MESSAGE_MAP(class_name, CComparerView)                \
		ON_WM_CREATE()                                          \
	END_MESSAGE_MAP()                                           \
                                                                \
	int class_name::OnCreate(LPCREATESTRUCT lpCreateStruct)     \
	{                                                           \
		if (CComparerView::OnCreate(lpCreateStruct) == -1)      \
			return -1;                                          \
                                                                \
		CComparerDoc* pDoc = GetDocument();                     \
		mPane = &pDoc->mPane[view_id];                          \
		mPane->pView = this;                                    \
                                                                \
		return 0;                                               \
	}

IMPLEMENT_COMPARER_VIEW(CComparerView1, CComparerDoc::IMG_VIEW_1)
IMPLEMENT_COMPARER_VIEW(CComparerView2, CComparerDoc::IMG_VIEW_2)
IMPLEMENT_COMPARER_VIEW(CComparerView3, CComparerDoc::IMG_VIEW_3)