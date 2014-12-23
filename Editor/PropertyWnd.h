#pragma once

class PropertyListener
{
public:
	/** ����ObjectEidtHandler���޸���PropertyWnd�����õ���Դʱ��
		Ӧ����ʹPropertyWnd�������е����ݡ�
	*/
	virtual void firePropertyChanged() = 0;

	/** ���޸���PropertyWnd�е����ԣ���������������õ���Դ��
	*/
	virtual void onPropertyChanged(CBCGPProp *prop) = 0;
};

class PropertyWnd : public CBCGPDockingControlBar
{
	DECLARE_DYNAMIC(PropertyWnd)
public:
	PropertyWnd();
	virtual ~PropertyWnd();

	CBCGPPropList *getPropList() { return &propList; }
	void setListener(PropertyListener *listener) { this->listener = listener; }
	void firePropertyChanged();

	static PropertyWnd *current;

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnPropertyChanged(WPARAM wparam, LPARAM lparam);

	CBCGPPropList propList;
	PropertyListener *listener;
};
