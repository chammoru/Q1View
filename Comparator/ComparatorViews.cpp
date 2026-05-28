#include "stdafx.h"
#include "Comparator.h"
#include "ComparatorDoc.h"
#include "ComparatorView.h"
#include "ComparatorViews.h"


#define IMPLEMENT_COMPARER_VIEW(class_name, view_id)            \
	IMPLEMENT_DYNCREATE(class_name, CView)                      \
                                                                \
	BEGIN_MESSAGE_MAP(class_name, CComparatorView)                \
		ON_WM_CREATE()                                          \
	END_MESSAGE_MAP()                                           \
                                                                \
	int class_name::OnCreate(LPCREATESTRUCT lpCreateStruct)     \
	{                                                           \
		if (CComparatorView::OnCreate(lpCreateStruct) == -1)      \
			return -1;                                          \
                                                                \
		CComparatorDoc* pDoc = GetDocument();                     \
		mPane = &pDoc->mPane[view_id];                          \
		mPane->pView = this;                                    \
                                                                \
		return 0;                                               \
	}

IMPLEMENT_COMPARER_VIEW(CComparatorView1, CComparatorDoc::IMG_VIEW_1)
IMPLEMENT_COMPARER_VIEW(CComparatorView2, CComparatorDoc::IMG_VIEW_2)
IMPLEMENT_COMPARER_VIEW(CComparatorView3, CComparatorDoc::IMG_VIEW_3)
IMPLEMENT_COMPARER_VIEW(CComparatorView4, CComparatorDoc::IMG_VIEW_4)
