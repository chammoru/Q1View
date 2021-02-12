#pragma once

#define DECLARE_COMPARER_VIEW(class_name)                    \
	class class_name : public CComparerView                  \
    {                                                        \
		DECLARE_DYNCREATE(class_name)                        \
	                                                         \
	public:                                                  \
		DECLARE_MESSAGE_MAP()                                \
		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct); \
	};

DECLARE_COMPARER_VIEW(CComparerView1)
DECLARE_COMPARER_VIEW(CComparerView2)
DECLARE_COMPARER_VIEW(CComparerView3)
DECLARE_COMPARER_VIEW(CComparerView4)
