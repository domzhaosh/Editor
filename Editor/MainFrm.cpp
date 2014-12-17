#include "stdafx.h"
#include "resource.h"
#include "MainFrm.h"
#include "RenderPump.h"

IMPLEMENT_DYNAMIC(CMainFrame, CBCGPMDIFrameWnd)

CMainFrame::CMainFrame()
{
}

CMainFrame::~CMainFrame()
{
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CBCGPMDIFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	return TRUE;
}

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CMDIFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CMDIFrameWnd::Dump(dc);
}
#endif

BEGIN_MESSAGE_MAP(CMainFrame, CBCGPMDIFrameWnd)
	ON_WM_CREATE()
	ON_WM_TIMER()
END_MESSAGE_MAP()

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CBCGPMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CBCGPMDITabParams mdiTabParams;
	mdiTabParams.m_style = CBCGPTabWnd::STYLE_3D_VS2005; // ����������ʽ...
	mdiTabParams.m_bActiveTabCloseButton = TRUE;
	mdiTabParams.m_bDocumentMenu = TRUE; // ��ѡ�������ұ�Ե�����ĵ��˵�
	EnableMDITabbedGroups(TRUE, mdiTabParams);

	//////////////////////////////////////////////////
	// ������
	//////////////////////////////////////////////////

	InitScintilla();
	InitOGRE();

	CreateToolBar();
	CreateStatusBar();
	CreateDockingWindows();

	//CBCGPVisualManager::SetDefaultManager(RUNTIME_CLASS(CBCGPVisualManager2013));
	//CBCGPVisualManager2013::SetStyle(CBCGPVisualManager2013::Style::Office2013_Gray);
	CBCGPDockManager::SetDockMode(BCGP_DT_SMART);
	SetTimer(1, 30, NULL);

	return 0;
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	if(nIDEvent == 1)
	{
		mRenderPump.renderOneFrame();
	}
}

void CMainFrame::CreateToolBar()
{
	mObjectEditToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
	mObjectEditToolBar.LoadToolBar(IDR_OBJECT_EDIT);

	mObjectEditToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&mObjectEditToolBar);

	CBCGPToolBar::ResetAllImages();
	mObjectEditToolBar.LoadBitmap(IDB_OBJECT_EDIT);
}

void CMainFrame::CreateStatusBar()
{
	static UINT Indicators[] =
	{
		ID_SEPARATOR,           
		ID_INDICATOR_CAPS,
		ID_INDICATOR_NUM,
		ID_INDICATOR_SCRL,
	};

	mStatusBar.Create(this);
	mStatusBar.SetIndicators(Indicators, sizeof(Indicators)/sizeof(UINT));
}

void CMainFrame::CreateDockingWindows()
{
	CBCGPToolBarImages imagesWorkspace;
	imagesWorkspace.SetImageSize(CSize(16, 16));
	imagesWorkspace.SetTransparentColor(RGB(192, 192, 192));
	imagesWorkspace.Load(IDB_DOCKING_WINDOW);

	// ������Դ����������
	mResourceManager.Create("��Դ����", this, CRect(0, 0, 200, 200), TRUE, ID_RESOURCE_MANAGER, 
		WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|CBRS_LEFT|CBRS_FLOAT_MULTI);
	mResourceManager.SetIcon(imagesWorkspace.ExtractIcon(0), FALSE);
	mResourceManager.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&mResourceManager);

	// ����������Դ����
	mSceneResource.Create("������Դ", this, CRect(0, 0, 200, 200), TRUE, ID_SCENE_RESOURCE, 
		WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|CBRS_LEFT|CBRS_FLOAT_MULTI);
	mSceneResource.SetIcon(imagesWorkspace.ExtractIcon(1), FALSE);
	mSceneResource.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&mSceneResource);

	// ������ϱ༭����
	mMiscEditWnd.Create("��ϱ༭", this, CRect(0, 0, 200, 200), TRUE, ID_MISC_EDIT_WND,
		WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|CBRS_LEFT|CBRS_FLOAT_MULTI);
	mMiscEditWnd.EnableDocking(CBRS_ALIGN_ANY);
	mMiscEditWnd.SetIcon(imagesWorkspace.ExtractIcon(2), FALSE);
	DockControlBar(&mMiscEditWnd);

	mSceneResource.AttachToTabWnd(&mResourceManager, BCGP_DM_STANDARD, FALSE, NULL);
	mMiscEditWnd.AttachToTabWnd(&mResourceManager, BCGP_DM_STANDARD, FALSE, NULL);

	// �������Դ���
	mPropertyWnd.Create("����", this, CRect(0, 0, 200, 200), TRUE, ID_PROPERTY_WND, 
		WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|CBRS_RIGHT|CBRS_FLOAT_MULTI);
	mPropertyWnd.SetIcon(imagesWorkspace.ExtractIcon(4), FALSE);
	mPropertyWnd.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&mPropertyWnd);

	// �����������
	mCameraWnd.Create("���", this, CRect(0, 0, 200, 200), TRUE, ID_CAMERA_WND, 
		WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|CBRS_RIGHT|CBRS_FLOAT_MULTI);
	mCameraWnd.SetIcon(imagesWorkspace.ExtractIcon(3), FALSE);
	mCameraWnd.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&mCameraWnd);

	// ������ԴԤ������
	mResourcePreview.Create("��ԴԤ��", this, CRect(0, 0, 200, 200), TRUE, ID_RESOURCE_PREVIEW, 
		WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|CBRS_LEFT|CBRS_FLOAT_MULTI);
	mResourcePreview.SetIcon(imagesWorkspace.ExtractIcon(5), FALSE);
	mResourcePreview.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&mResourcePreview);

	mResourcePreview.DockToWindow(&mPropertyWnd, CBRS_ALIGN_BOTTOM);
	mCameraWnd.DockToWindow(&mPropertyWnd, CBRS_ALIGN_BOTTOM);

	// �����������
	mOutputWnd.Create("���", this, CRect(0, 0, 200, 200), TRUE, ID_RESOURCE_PREVIEW, 
		WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|CBRS_BOTTOM|CBRS_FLOAT_MULTI);
	mOutputWnd.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&mOutputWnd);
}

void CMainFrame::InitScintilla()
{
	LoadLibrary("SciLexer.dll");
}

void CMainFrame::InitOGRE()
{
	mRenderPump.initialize();
}
