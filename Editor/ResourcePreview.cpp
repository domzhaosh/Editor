#include "stdafx.h"
#include "resource.h"
#include "ResourcePreview.h"

#include "CameraManager.h"
#include "DynamicModel.h"
#include "Particle.h"
#include "RenderView.h"
#include "ResourceManagerTree.h"
#include "StaticMesh.h"
#include "StringUtils.h"

#include "OgreCamera.h"
#include "OgreManualObject.h"
#include "OgreRoot.h"
#include "OgreSceneManager.h"

#include "tinyxml.h"

IMPLEMENT_DYNAMIC(ResourcePreview, CBCGPDockingControlBar)

ResourcePreview *ResourcePreview::Current = NULL;
ResourcePreview::ResourcePreview():
	mView(NULL),
	mShowGrid(false),
	mSceneManager(NULL),
	mCamera(NULL),
	mGrid(NULL),
	mSceneObject(NULL)
{
	Current = this;
}

ResourcePreview::~ResourcePreview()
{
}

void ResourcePreview::AfterSelectResource(TiXmlElement *Elmt)
{
	if(mSceneManager == NULL)
	{
		mSceneManager = Ogre::Root::getSingleton().createSceneManager(Ogre::ST_GENERIC);
		mSceneManager->setAmbientLight(Ogre::ColourValue::White); // ��׹⣬��������
		mCamera = mSceneManager->createCamera("Camera");
		mCameraManager.setCamera(mCamera);
		mCameraManager.setMode(CameraManager::ORBIT);

		mCamera->setPosition(Ogre::Vector3(0, 100, 100));
		mCamera->lookAt(Ogre::Vector3(0, -1, -1));
		mCamera->setNearClipDistance(0.1f);
		mCamera->setFarClipDistance(10000.0f);
		if(Ogre::Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(Ogre::RSC_INFINITE_FAR_PLANE))
		{
			mCamera->setFarClipDistance(0);   // enable infinite far clip distance if we can
		}

		mView->setCameraManager(&mCameraManager);
	}
	ClearSceneObject();
	KResourceManagerTreeImages Type;
	Elmt->QueryIntAttribute("Type", (int*)&Type);
	switch(Type)
	{
	case RMTI_MESH:
		{
			mSceneObject = new StaticMesh(mSceneManager);
			mSceneObject->create(Elmt->Attribute("Path"), Elmt->Attribute("Path"));
			AdjustCamera();
		}
		break;

	case RMTI_MODEL:
		{
			mSceneObject = new DynamicModel(mSceneManager);
			mSceneObject->create(Elmt->Attribute("Path"), Elmt->Attribute("Path"));
			AdjustCamera();
		}
		break;

	case RMTI_PARTICLE:
		{
			mSceneObject = new Particle(mSceneManager);

			std::string TemplateName = StringUtils::filename(Elmt->Attribute("Path"));
			mSceneObject->create(Elmt->Attribute("Path"), TemplateName);
			AdjustCamera();
		}
	}
	mCameraManager.setTarget(mSceneObject);
}

void ResourcePreview::update(float Elapsed)
{
	if(mSceneObject != NULL)
		mSceneObject->update(Elapsed);
	mView->Invalidate(FALSE);
}

BEGIN_MESSAGE_MAP(ResourcePreview, CBCGPDockingControlBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_COMMAND(ID_SHOW_GRID, OnShowGrid)
    ON_UPDATE_COMMAND_UI(ID_SHOW_GRID, OnUpdateShowGrid)
END_MESSAGE_MAP()

int ResourcePreview::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CBCGPDockingControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;

	mView = (RenderView*)RUNTIME_CLASS(RenderView)->CreateObject();
  	mView->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), this, 1, NULL);

	mToolbar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE, IDR_RESOURCE_PREVIEW);
	mToolbar.LoadToolBar(IDR_RESOURCE_PREVIEW, 0, 0, TRUE /* ������*/);
	mToolbar.SetBarStyle(mToolbar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
	mToolbar.SetBarStyle(mToolbar.GetBarStyle() & ~(CBRS_GRIPPER | CBRS_SIZE_DYNAMIC | CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM | CBRS_BORDER_LEFT | CBRS_BORDER_RIGHT));
	mToolbar.SetOwner(this);

	mToolbar.SetRouteCommandsViaFrame(FALSE);
	return 0;
}


void ResourcePreview::OnSize(UINT nType, int cx, int cy)
{
	CBCGPDockingControlBar::OnSize(nType, cx, cy);

	if (GetSafeHwnd() == NULL)
	{
		return;
	}

	CRect rectClient;
	GetClientRect(rectClient);

	int cyTlb = mToolbar.CalcFixedLayout(FALSE, TRUE).cy;

	mToolbar.SetWindowPos(NULL, rectClient.left, rectClient.top, rectClient.Width(), cyTlb, SWP_NOACTIVATE | SWP_NOZORDER);
	mView->SetWindowPos(NULL, rectClient.left, rectClient.top + cyTlb, rectClient.Width(), rectClient.Height() - cyTlb, SWP_NOACTIVATE | SWP_NOZORDER);

}

void ResourcePreview::OnShowGrid()
{
	mShowGrid = !mShowGrid;
	ShowGrid();
}

void ResourcePreview::OnUpdateShowGrid(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(mShowGrid);
}

BOOL ResourcePreview::PreTranslateMessage(MSG* pMsg)
{
 	if (pMsg->message == WM_CHAR) // ���������
 	{
 		::TranslateMessage(pMsg);
 		::DispatchMessage(pMsg);
 		return TRUE;
 	}
	return CBCGPDockingControlBar::PreTranslateMessage(pMsg);
}

void ResourcePreview::ClearSceneObject()
{
	if(mSceneObject != NULL)
	{
		mCameraManager.setTarget(NULL);

		mSceneObject->destroy();
		delete mSceneObject;
		mSceneObject = NULL;
	}
}

void ResourcePreview::AdjustCamera()
{
	mCameraManager.faceTo(mSceneObject);
}

void ResourcePreview::ShowGrid()
{
	if(mGrid == NULL)
	{
		mGrid = mSceneManager->createManualObject("<ShowResource_Horizontal>");
		mGrid->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_LIST);
		// �������Ϊ���
		float Width = 1.0f, Height = 1.0f;
		float Step = 0.2f;
		for(float x = -Width; x < Width; x += Step)
		{
			mGrid->position(x, 0, -Height);
			mGrid->position(x, 0, Height);
		}
		for(float z = -Height; z < Height; z += Step)
		{
			mGrid->position(-Width, 0, z);
			mGrid->position(Width, 0, z);
		}
		mGrid->end();
		mSceneManager->getRootSceneNode()->createChildSceneNode()->attachObject(mGrid);
	}
	mGrid->setVisible(mShowGrid);
	if(mSceneObject != NULL)
	{
		float R = mSceneObject->getBoundingRadius();
 		mGrid->getParentNode()->setScale(R, R, R);
	}
}
