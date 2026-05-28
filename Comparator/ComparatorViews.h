#pragma once

#define DECLARE_COMPARER_VIEW(class_name)                    \
	class class_name : public CComparatorView                  \
	{                                                        \
		DECLARE_DYNCREATE(class_name)                        \
	                                                         \
	public:                                                  \
		DECLARE_MESSAGE_MAP()                                \
		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct); \
	};

DECLARE_COMPARER_VIEW(CComparatorView1)
DECLARE_COMPARER_VIEW(CComparatorView2)
DECLARE_COMPARER_VIEW(CComparatorView3)
DECLARE_COMPARER_VIEW(CComparatorView4)
