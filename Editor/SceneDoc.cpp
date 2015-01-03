#include "stdafx.h"
#include "resource.h"

#include "MainFrm.h"
#include "SceneDoc.h"
#include "SceneView.h"
#include "RenderPump.h"

#include "DynamicModel.h"
#include "Light.h"
#include "LiquidEditHandler.h"
#include "NewLightDlg.h"
#include "NewSceneDlg.h"
#include "ObjectEditHandler.h"
#include "ResourceManagerTree.h"
#include "SceneObject.h"
#include "SceneResourceTree.h"
#include "StaticMesh.h"
#include "TerrainEditHandler.h"
#include "DeferredShading.h"
#include "TerrainManager.h"

#include "OgreCamera.h"
#include "OgreEntity.h"
#include "OgreGpuProgramManager.h"
#include "OgreLight.h"
#include "OgreOverlaySystem.h"
#include "OgreRay.h"
#include "OgreRenderWindow.h"
#include "OgreRoot.h"
#include "OgreSceneManager.h"
#include "OgreShadowCameraSetupPSSM.h"
#include "OgreSubEntity.h"
#include "OgreTerrainGroup.h"
#include "OgreTerrainMaterialGeneratorA.h"
#include "TerrainMaterialGeneratorB.h"
#include "TerrainMaterialGeneratorC.h"

#include "tinyxml.h"

IMPLEMENT_DYNCREATE(SceneDoc, CDocument)

SceneDoc *SceneDoc::current = NULL;
SceneDoc::SceneDoc()
{
	editMode = 0;
	paste = false;
	showDebugOverlay = false;
	initialized = false;

	// Deferred Shading
	deferredShadingSystem = NULL;
	active = false; shadow = false; ssao = false;
	deferredShadingMode = ID_DEFERREDSHADING_REGULARVIEW;

	activeView = NULL;
	terrainManager = NULL;
	terrainManagerConfig = NULL;
	terrainEditHandler = NULL;
	objectEditHandler = NULL;
	liquidEditHandler = NULL;

	nameID = 0;
}

SceneDoc::~SceneDoc()
{
}

//Utility function to help set scene up
static void setEntityHeight(Ogre::Entity* ent, Ogre::Real newHeight)
{
    Ogre::Real curHeight = ent->getMesh()->getBounds().getSize().y;
    Ogre::Real scaleFactor = newHeight / curHeight;
        
    Ogre::SceneNode* parentNode = ent->getParentSceneNode();
    parentNode->setScale(scaleFactor, scaleFactor, scaleFactor);
}

void SceneDoc::initialize(NewSceneDlg *dlg)
{
	sceneName = dlg->getProperty(NewSceneDlg::SCENE_NAME);
	SetTitle(sceneName);

	sceneManager = Ogre::Root::getSingleton().createSceneManager(Ogre::ST_GENERIC); 
	sceneManager->addRenderQueueListener(RenderPump::current->getOverlaySystem());

	// Ŀǰ��֧�־۹��
	// Create main, static light
	//Ogre::Light* l1 = sceneManager->createLight();
	//l1->setType(Ogre::Light::LT_DIRECTIONAL);
	//l1->setDiffuseColour(0.5f, 0.45f, 0.1f);
	//l1->setDirection(1, -0.5, -0.2);
	//l1->setShadowFarClipDistance(3000);
	//l1->setShadowFarDistance(75);
	////Turn this on to have the directional light cast shadows
	//l1->setCastShadows(false);

	COLORREF refAmbientLight = dlg->getProperty(NewSceneDlg::AMBIENT_LIGHT);
	sceneManager->setAmbientLight(
		Ogre::ColourValue(GetRValue(refAmbientLight)/255.0f, GetGValue(refAmbientLight)/255.0f, GetBValue(refAmbientLight)/255.0f));
	
	skyType = dlg->getProperty(NewSceneDlg::SKY_TYPE);
	if(skyType != "None")
	{
		skyMaterial = dlg->getProperty(NewSceneDlg::SKY_MATERIAL);
		if(skyType == "Box")
		{
			sceneManager->setSkyBox(true, Ogre::String(skyMaterial));
		} else 
		if(skyType == "Dome")
		{
			sceneManager->setSkyDome(true, Ogre::String(skyMaterial));
		} else 
		if(skyType == "Plane")
		{
			Ogre::Plane Plane;
			Plane.d=1000;
			Plane.normal=Ogre::Vector3::NEGATIVE_UNIT_Y;

			sceneManager->setSkyPlane(true, Plane, Ogre::String(skyMaterial));
		}
	}

	CString strFogMode = dlg->getProperty(NewSceneDlg::FOG_MODE);
	if(strFogMode == "None")
	{
		sceneManager->setFog();
	}
	else
	{
		Ogre::FogMode FogMode;
		if(strFogMode == "Linear")
		{
			FogMode = Ogre::FOG_LINEAR;
		} else 
		if(strFogMode == "Exp")
		{
			FogMode = Ogre::FOG_EXP;
		} else
		if(strFogMode == "Exp2")
		{
			FogMode = Ogre::FOG_EXP2;
		}

		COLORREF refFogColour = dlg->getProperty(NewSceneDlg::FOG_COLOUR);
		Ogre::ColourValue FogColour(GetRValue(refFogColour)/255.0f, GetGValue(refFogColour)/255.0f, GetBValue(refFogColour)/255.0f);

		Ogre::Real FogDensity = dlg->getProperty(NewSceneDlg::FOG_DENSITY);
		Ogre::Real FogStart = dlg->getProperty(NewSceneDlg::FOG_START);
		Ogre::Real FogEnd = dlg->getProperty(NewSceneDlg::FOG_END);
		sceneManager->setFog(FogMode, FogColour, FogDensity, FogStart, FogEnd);
	}

	camera = sceneManager->createCamera(
		Ogre::StringConverter::toString(Ogre::Math::RangeRandom(0, 1000)) + ".Camera");
	cameraManager.setCamera(camera);
	cameraManager.setDragLook(TRUE);

	camera->setNearClipDistance(0.5f);
	camera->setFarClipDistance(10000.0f);
	//if(Ogre::Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(Ogre::RSC_INFINITE_FAR_PLANE))
	//{
	//	camera->setFarClipDistance(0);
	//}

	//////////////////////////////////////////////////
	// Terrain
	//////////////////////////////////////////////////

	terrainManagerConfig = new TerrainManagerConfig;

	Ogre::Real LightMapDirectionX = dlg->getProperty(NewSceneDlg::LIGHT_MAP_DIRECTION_X);
	Ogre::Real LightMapDirectionY = dlg->getProperty(NewSceneDlg::LIGHT_MAP_DIRECTION_Y);
	Ogre::Real LightMapDirectionZ = dlg->getProperty(NewSceneDlg::LIGHT_MAP_DIRECTION_Z);
	terrainManagerConfig->LightMapDirection = Ogre::Vector3(LightMapDirectionX, LightMapDirectionY, LightMapDirectionZ);
	terrainManagerConfig->LightMapSize = dlg->getProperty(NewSceneDlg::LIGHT_MAP_SIZE);

	terrainManagerConfig->CompositeMapAmbient = sceneManager->getAmbientLight();
	COLORREF refCompositeMapDiffuse = dlg->getProperty(NewSceneDlg::COMPOSITE_MAP_DIFFUSE);
	terrainManagerConfig->CompositeMapDiffuse = 
		Ogre::ColourValue(GetRValue(refCompositeMapDiffuse)/255.0f, GetGValue(refCompositeMapDiffuse)/255.0f, GetBValue(refCompositeMapDiffuse)/255.0f);
	terrainManagerConfig->CompositeMapDistance = dlg->getProperty(NewSceneDlg::COMPOSITE_MAP_DISTANCE);
	terrainManagerConfig->CompositeMapSize = dlg->getProperty(NewSceneDlg::COMPOSITE_MAP_SIZE);

	terrainManagerConfig->FilenamePrefix = 
		Ogre::String((CString)dlg->getProperty(NewSceneDlg::FILENAME_PREFIX));
	terrainManagerConfig->FilenameExtension = 
		Ogre::String((CString)dlg->getProperty(NewSceneDlg::FILENAME_EXTENSION));

	terrainManagerConfig->TerrainSize = dlg->getProperty(NewSceneDlg::TERRAIN_SIZE);
	terrainManagerConfig->WorldSize = dlg->getProperty(NewSceneDlg::WORLD_SIZE);
	terrainManagerConfig->InputScale = dlg->getProperty(NewSceneDlg::INPUT_SCALE);
	terrainManagerConfig->MinBatchSize = dlg->getProperty(NewSceneDlg::MIN_BATCH_SIZE);
	terrainManagerConfig->MaxBatchSize = dlg->getProperty(NewSceneDlg::MAX_BATCH_SIZE);
	
	terrainManagerConfig->LayerCount = 1;
	terrainManagerConfig->TextureWorldSizes.push_back(dlg->getProperty(NewSceneDlg::TEXTURE_WORLD_SIZE));
	terrainManagerConfig->DiffuseSpeculars.push_back(
		Ogre::String((CString)dlg->getProperty(NewSceneDlg::DIFFUSE_SPECULAR)));
	terrainManagerConfig->NormalHeights.push_back(
		Ogre::String((CString)dlg->getProperty(NewSceneDlg::NORMAL_HEIGHT)));

	terrainManagerConfig->LoadFromFile = false;
	terrainManagerConfig->HeightMap = 
		Ogre::String((CString)dlg->getProperty(NewSceneDlg::HEIGHT_MAP));

	terrainManager = new TerrainManager(this, terrainManagerConfig);
	terrainEditHandler = new TerrainEditHandler(this);
	objectEditHandler = new ObjectEditHandler(this);
	liquidEditHandler = new LiquidEditHandler(this);

	//Ogre::Entity* knotEnt = sceneManager->createEntity("Knot", "knot.mesh");
	//knotEnt->setMaterialName("DeferredDemo/RockWall");
	//Ogre::SceneNode *knotNode = sceneManager->getRootSceneNode()->createChildSceneNode();
	//knotNode->setPosition(0, 2, 0);
	//knotNode->attachObject(knotEnt);
	//setEntityHeight(knotEnt, 3);

	//Ogre::Light* knotLight = sceneManager->createLight("KnotLight1");
	//knotLight->setType(Ogre::Light::LT_SPOTLIGHT);
	//knotLight->setDiffuseColour(Ogre::ColourValue::Red);
	//knotLight->setSpecularColour(Ogre::ColourValue::White);
	//knotLight->setPosition(knotNode->getPosition() + Ogre::Vector3(0,3,0));
	//knotLight->setDirection(Ogre::Vector3::NEGATIVE_UNIT_Y);
	//knotLight->setSpotlightRange(Ogre::Degree(25), Ogre::Degree(45), 1);
	//knotLight->setAttenuation(6, 1, 0.2, 0);

	OnShowDebugOverlay();
	CBCGPToolBar *ToolBar = ((MainFrame*)AfxGetMainWnd())->getObjectEditToolBar();
	ToolBar->OnUpdateCmdUI((CFrameWnd*)ToolBar->GetOwner(), FALSE);

	initialized = TRUE;
}

void SceneDoc::initialize(CString Filename)
{
	TiXmlDocument *Doc = new TiXmlDocument;
	Doc->LoadFile(Filename);

	const TiXmlElement *Root = Doc->RootElement(), *Elmt = NULL, *SubElmt = NULL;
	sceneName = Root->Value();
	SetTitle(sceneName);

	sceneManager = Ogre::Root::getSingleton().createSceneManager(Ogre::ST_GENERIC);
	sceneManager->addRenderQueueListener(RenderPump::current->getOverlaySystem());

	Ogre::ColourValue AmbientLight;
	sscanf(Root->Attribute("AmbientLight"), "%f,%f,%f", &AmbientLight.r, &AmbientLight.g, &AmbientLight.b);
	sceneManager->setAmbientLight(AmbientLight);

	Elmt = Root->FirstChildElement();
	CString strFogMode = Elmt->Attribute("Mode");
	if(strFogMode == "None")
	{
		sceneManager->setFog();
	}
	else
	{
		Ogre::FogMode FogMode;
		if(strFogMode == "Linear")
		{
			FogMode = Ogre::FOG_LINEAR;
		} else 
		if(strFogMode == "Exp")
		{
			FogMode = Ogre::FOG_EXP;
		} else
		if(strFogMode == "Exp2")
		{
			FogMode = Ogre::FOG_EXP2;
		}

		Ogre::ColourValue FogColour;
		sscanf(Root->Attribute("Colour"), "%f,%f,%f", &FogColour.r, &FogColour.g, &FogColour.b);
	
		Ogre::Real FogDensity, FogStart, FogEnd;
		sscanf(Root->Attribute("Density"), "%f", &FogDensity);
		sscanf(Root->Attribute("Start"), "%f", &FogStart);
		sscanf(Root->Attribute("End"), "%f", &FogEnd);
		sceneManager->setFog(FogMode, FogColour, FogDensity, FogStart, FogEnd);
	}

	Elmt = Elmt->NextSiblingElement();
	skyType = Elmt->Attribute("Type");
	if(skyType != "None")
	{
		skyMaterial = Elmt->Attribute("Material");
		if(skyType == "Box")
		{
			sceneManager->setSkyBox(true, Ogre::String(skyMaterial));
		} else 
		if(skyType == "Dome")
		{
			sceneManager->setSkyDome(true, Ogre::String(skyMaterial), 5, 8, 500);
		} else 
		if(skyType == "Plane")
		{
			Ogre::Plane Plane;
			Plane.d=1000;
			Plane.normal=Ogre::Vector3::NEGATIVE_UNIT_Y;

			sceneManager->setSkyPlane(true, Plane, Ogre::String(skyMaterial));
		}
	}

	camera = sceneManager->createCamera(
		Ogre::StringConverter::toString(Ogre::Math::RangeRandom(0, 1000)) + ".Camera");
	cameraManager.setCamera(camera);
	cameraManager.setDragLook(TRUE);

    camera->setNearClipDistance(1.0f);
    camera->setFarClipDistance(10000.0f);
	if(Ogre::Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(Ogre::RSC_INFINITE_FAR_PLANE))
	{
		camera->setFarClipDistance(0);
	}

	//////////////////////////////////////////////////
	// Terrain manager config
	//////////////////////////////////////////////////

	terrainManagerConfig = new TerrainManagerConfig;

	Elmt = Elmt->NextSiblingElement();
	SubElmt = Elmt->FirstChildElement();

	sscanf(SubElmt->Attribute("Direction"), "%f,%f,%f", &terrainManagerConfig->LightMapDirection.x,
		&terrainManagerConfig->LightMapDirection.y, &terrainManagerConfig->LightMapDirection.z);
	sscanf(SubElmt->Attribute("Size"), "%u", &terrainManagerConfig->LightMapSize);

	SubElmt = SubElmt->NextSiblingElement();

	sscanf(SubElmt->Attribute("Ambient"), "%f,%f,%f", &terrainManagerConfig->CompositeMapAmbient.r,
		&terrainManagerConfig->CompositeMapAmbient.g, &terrainManagerConfig->CompositeMapAmbient.b);
	sscanf(SubElmt->Attribute("Diffuse"), "%f,%f,%f", &terrainManagerConfig->CompositeMapDiffuse.r,
		&terrainManagerConfig->CompositeMapDiffuse.g, &terrainManagerConfig->CompositeMapDiffuse.b);
	sscanf(SubElmt->Attribute("Distance"), "%f", &terrainManagerConfig->CompositeMapDistance);
	sscanf(SubElmt->Attribute("Size"), "%u", &terrainManagerConfig->CompositeMapSize);

	SubElmt = SubElmt->NextSiblingElement();
	
	terrainManagerConfig->FilenamePrefix = SubElmt->Attribute("FilenamePrefix");
	terrainManagerConfig->FilenameExtension = SubElmt->Attribute("FilenameExtension");
	sscanf(SubElmt->Attribute("TerrainSize"), "%f", &terrainManagerConfig->TerrainSize);
	sscanf(SubElmt->Attribute("WorldSize"), "%f", &terrainManagerConfig->WorldSize);
	sscanf(SubElmt->Attribute("InputScale"), "%f", &terrainManagerConfig->InputScale);
	sscanf(SubElmt->Attribute("MinBatchSize"), "%f", &terrainManagerConfig->MinBatchSize);
	sscanf(SubElmt->Attribute("MaxBatchSize"), "%f", &terrainManagerConfig->MaxBatchSize);

	SubElmt = SubElmt->NextSiblingElement();

	terrainManagerConfig->LayerCount = 0;
	while(SubElmt != NULL)
	{
		terrainManagerConfig->LayerCount++;

		Ogre::Real TextureWorldSize;
		sscanf(SubElmt->Attribute("TextureWorldSize"), "%f", &TextureWorldSize);
		Ogre::String DiffuseSpecular = SubElmt->Attribute("DiffuseSpecular");
		Ogre::String NormalHeight = SubElmt->Attribute("NormalHeight");
		terrainManagerConfig->TextureWorldSizes.push_back(TextureWorldSize);
		terrainManagerConfig->DiffuseSpeculars.push_back(DiffuseSpecular);
		terrainManagerConfig->NormalHeights.push_back(NormalHeight);

		SubElmt = SubElmt->NextSiblingElement();
	}

	terrainManagerConfig->LoadFromFile = true;
	terrainManager = new TerrainManager(this, terrainManagerConfig);

	//CString strShadowTechnique = dlg->getProperty(NSD_SHADOW_TECHNIQUE);
	//ConfigureShadows(strShadowTechnique != "None", strShadowTechnique == "Depth Shadows");

	terrainEditHandler = new TerrainEditHandler(this);
	objectEditHandler = new ObjectEditHandler(this);
	liquidEditHandler = new LiquidEditHandler(this);

	Elmt = Elmt->NextSiblingElement();
	SubElmt = Elmt->FirstChildElement();
	while(SubElmt != NULL)
	{
		CString strValue = SubElmt->Value();
		if(strValue == "StaticMesh")
		{
			StaticMesh *Object = addStaticMesh(SubElmt->Attribute("Path"));

			Ogre::Vector3 Position;
			sscanf(SubElmt->Attribute("Position"), "%f,%f,%f", &Position.x, &Position.y, &Position.z);
			Object->getSceneNode()->setPosition(Position);

			Ogre::Quaternion Direction;
			sscanf(SubElmt->Attribute("Direction"), "%f,%f,%f,%f", &Direction.w, &Direction.x, &Direction.y, &Direction.z);
			Object->getSceneNode()->setOrientation(Direction);

			Ogre::Vector3 Scale;
			sscanf(SubElmt->Attribute("Scale"), "%f,%f,%f", &Scale.x, &Scale.y, &Scale.z);
			Object->getSceneNode()->setScale(Scale);
		} else
		if(strValue == "DynamicModel")
		{
			DynamicModel *Object = addDynamicModel(SubElmt->Attribute("Path"));

			Ogre::Vector3 Position;
			sscanf(SubElmt->Attribute("Position"), "%f,%f,%f", &Position.x, &Position.y, &Position.z);
			Object->getSceneNode()->setPosition(Position);

			Ogre::Quaternion Direction;
			sscanf(SubElmt->Attribute("Direction"), "%f,%f,%f,%f", &Direction.w, &Direction.x, &Direction.y, &Direction.z);
			Object->getSceneNode()->setOrientation(Direction);

			Ogre::Vector3 Scale;
			sscanf(SubElmt->Attribute("Scale"), "%f,%f,%f", &Scale.x, &Scale.y, &Scale.z);
			Object->getSceneNode()->setScale(Scale);
		} else
		if(strValue == "Light")
		{
			Light *Object = addLight();

			CString strType = SubElmt->Attribute("Type");
			if(strType == "Point")
			{
				Object->getLight()->setType(Ogre::Light::LT_POINT);
			} else
			if(strType == "Directional")
			{
				Object->getLight()->setType(Ogre::Light::LT_DIRECTIONAL);
			} else
			if(strType == "Spotlight")
			{
				Object->getLight()->setType(Ogre::Light::LT_SPOTLIGHT);
			}

			Ogre::ColourValue DiffuseColour;
			sscanf(SubElmt->Attribute("DiffuseColour"), "%f,%f,%f", &DiffuseColour.r, &DiffuseColour.g, &DiffuseColour.b);
			Object->getLight()->setDiffuseColour(DiffuseColour);

			Ogre::ColourValue SpecularColour;
			sscanf(SubElmt->Attribute("SpecularColour"), "%f,%f,%f", &SpecularColour.r, &SpecularColour.g, &SpecularColour.b);
			Object->getLight()->setSpecularColour(SpecularColour);

			Ogre::Vector3 Position;
			sscanf(SubElmt->Attribute("Position"), "%f,%f,%f", &Position.x, &Position.y, &Position.z);
			Object->getSceneNode()->setPosition(Position);

			Ogre::Vector3 Direction;
			sscanf(SubElmt->Attribute("Direction"), "%f,%f,%f", &Direction.x, &Direction.y, &Direction.z);
			Object->getLight()->setDirection(Direction);

			float AttenuationRange;
			float AttenuationConstant;
			float AttenuationLinear;
			float AttenuationQuadratic;
			sscanf(SubElmt->Attribute("Attenuation"), "%f,%f,%f,%f", &AttenuationRange, &AttenuationConstant, &AttenuationLinear, &AttenuationQuadratic);
			Object->getLight()->setAttenuation(AttenuationRange, AttenuationConstant, AttenuationLinear, AttenuationQuadratic);

			float SpotlightInnerAngle;
			float SpotlightOuterAngle;
			float SpotlightFalloff;
			sscanf(SubElmt->Attribute("SpotlightRange"), "%f,%f,%f", &SpotlightInnerAngle, &SpotlightOuterAngle, &SpotlightFalloff);
			Object->getLight()->setSpotlightRange(Ogre::Radian(SpotlightInnerAngle), Ogre::Radian(SpotlightOuterAngle), SpotlightFalloff);
		}
		SubElmt = SubElmt->NextSiblingElement();
	}

	initialized = TRUE;
}

void SceneDoc::destroy()
{
	if(initialized)
	{
		delete liquidEditHandler;
		delete objectEditHandler;
		delete terrainEditHandler;
		delete terrainManagerConfig;
		delete terrainManager;
		Ogre::Root::getSingleton().destroySceneManager(sceneManager);
	}
}

void SceneDoc::roaming(CPoint ScreenPoint, float Elapsed)
{
	if(!initialized)
		return;

	Ogre::Ray mouseRay = camera->getCameraToViewportRay(
		ScreenPoint.x/(float)activeView->getWidth(), ScreenPoint.y/(float)activeView->getHeight());
	Ogre::TerrainGroup::RayResult rayResult = 
		terrainManager->GetTerrainGroup()->rayIntersects(mouseRay);

	if(terrainEditHandler->GetMode() != TEM_NONE)
	{
		if(!rayResult.hit)
			return;
		terrainEditHandler->Roaming(rayResult, Elapsed);
	} else 
	if(editMode == ID_LIQUID_EDIT)
	{
		if(!rayResult.hit)
			return;
		liquidEditHandler->Roaming(rayResult.position);
	}

	objectEditHandler->Roaming(ScreenPoint, rayResult, Elapsed);
}

void SceneDoc::leftDown(UINT nFlags, CPoint point)
{
	if(!initialized)
		return;

	Ogre::Ray mouseRay = camera->getCameraToViewportRay(
		point.x/(float)activeView->getWidth(), point.y/(float)activeView->getHeight());
	Ogre::TerrainGroup::RayResult rayResult = 
		terrainManager->GetTerrainGroup()->rayIntersects(mouseRay);

	if(editMode == ID_LIQUID_EDIT)
	{
		if(!rayResult.hit)
			return;
		liquidEditHandler->OnLButtonDown(rayResult);
	}
	else
	{
		objectEditHandler->OnLButtonDown(point);
	}
}

void SceneDoc::leftUp(UINT nFlags, CPoint point)
{
	if(!initialized)
		return;
	objectEditHandler->OnLButtonUp(point);
}

void SceneDoc::update(float Elapsed)
{
	if(initialized)
	{
		objectEditHandler->update(Elapsed);
		terrainEditHandler->update(Elapsed);
		liquidEditHandler->update(Elapsed);

		for(int i=0; i<objects.size(); i++)
			objects[i]->update(Elapsed);
	}
	POSITION ViewPos = GetFirstViewPosition();
	while(ViewPos != NULL)
	{
		((SceneView*)GetNextView(ViewPos))->Invalidate(FALSE);
	}
}

StaticMesh *SceneDoc::addStaticMesh(CString Path)
{
	CString Name; Name.Format("%s_%d", Path, nameID++);
	
	StaticMesh *staticMesh = new StaticMesh(sceneManager);
	staticMesh->create(Name.GetString(), Path.GetString());
	objects.push_back(staticMesh);

	HTREEITEM hItem = SceneResourceTree::current->InsertItem(
		TVIF_IMAGE|TVIF_SELECTEDIMAGE|TVIF_TEXT|TVIF_PARAM, 
		Name, SceneResourceTree::MESH, SceneResourceTree::MESH, 0, 0, LPARAM(staticMesh), 
		SceneResourceTree::current->getTreeItem(SceneResourceTree::MESH_SET), TVI_LAST);
	staticMesh->setUserAny(Ogre::Any(hItem));

	SceneResourceTree::current->SelectItem(hItem);
	objectEditHandler->SetFreeTransform(TRUE);

	return staticMesh;
}

DynamicModel *SceneDoc::addDynamicModel(CString Path)
{
	CString Name; Name.Format("%s_%d", Path, nameID++);

	DynamicModel *dynamicModel = new DynamicModel(sceneManager);
	dynamicModel->create(Name.GetString(), Path.GetString());
	objects.push_back(dynamicModel);
	
	HTREEITEM hItem = SceneResourceTree::current->InsertItem(
		TVIF_IMAGE|TVIF_SELECTEDIMAGE|TVIF_TEXT|TVIF_PARAM, 
		Name, SceneResourceTree::MODEL, SceneResourceTree::MODEL, 0, 0, LPARAM(dynamicModel), 
		SceneResourceTree::current->getTreeItem(SceneResourceTree::MESH_SET), TVI_LAST);
	dynamicModel->setUserAny(Ogre::Any(hItem));

	SceneResourceTree::current->SelectItem(hItem);
	objectEditHandler->SetFreeTransform(TRUE);

	return dynamicModel;
}

Light *SceneDoc::addLight()
{
	CString Name;
	Name.Format("Light_%d", nameID++);

	Light *light = new Light(sceneManager);
	light->create(Name.GetString(), "");
	objects.push_back(light);

	HTREEITEM hItem = SceneResourceTree::current->InsertItem(
		TVIF_IMAGE|TVIF_SELECTEDIMAGE|TVIF_TEXT|TVIF_PARAM, 
		Name, SceneResourceTree::LIGHT, SceneResourceTree::LIGHT, 0, 0, LPARAM(light), 
		SceneResourceTree::current->getTreeItem(SceneResourceTree::LIGHT_SET), TVI_LAST);
	light->setUserAny(Ogre::Any(hItem));
	if(hItem != NULL)
	{
		SceneResourceTree::current->SelectItem(hItem);
		objectEditHandler->SetFreeTransform(TRUE);
	}

	return light;
}

void SceneDoc::selectObject(SceneObject *pObject)
{
	if(!initialized)
		return;

 	if (!pObject)
	{
		objectEditHandler->SetTarget(NULL);
		objectEditHandler->SetMode(OEM_NONE);
		return;
	}

	objectEditHandler->SetTarget(pObject);
	objectEditHandler->SetMode(OEM_TRANS);
}

void SceneDoc::removeObject(SceneObject *Object)
{
	for(int i=0; i<objects.size(); i++)
	{
		if(objects[i] == Object)
		{
			objects[i]->destroy();
			objects.erase(objects.begin()+i);
			break;
		}
	}
}

void buildDepthShadowMaterial(SceneObject *SceneObject)
{
	Ogre::Entity *Entity = SceneObject->getEntity();
	if(Entity == NULL)
		return;

	for(int i=0; i<Entity->getNumSubEntities(); i++)
	{
		Ogre::SubEntity *SubEntity = Entity->getSubEntity(i);
		const Ogre::MaterialPtr &OldMaterial = SubEntity->getMaterial();
		Ogre::String TextureName = OldMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->getTextureName();

		Ogre::String NewMaterialName = "DepthShadows/" + TextureName;
		Ogre::MaterialPtr NewMaterial = Ogre::MaterialManager::getSingleton().getByName(NewMaterialName);
		if(NewMaterial.isNull())
		{
			Ogre::MaterialPtr BaseMaterial = Ogre::MaterialManager::getSingleton().getByName("Ogre/shadow/depth/integrated/pssm");
			NewMaterial = BaseMaterial->clone(NewMaterialName);
			Ogre::Pass *p = NewMaterial->getTechnique(0)->getPass(0);
			p->getTextureUnitState("diffuse")->setTextureName(TextureName);
		}

		SubEntity->setMaterial(NewMaterial);
	}
}

void SceneDoc::configureShadows(CString shadowTechnique)
{
	bool enabled = shadowTechnique != "None";
	bool depthShadows = shadowTechnique == "Depth Shadows";

	Ogre::TerrainMaterialGeneratorA::SM2Profile* matProfile = 
		static_cast<Ogre::TerrainMaterialGeneratorA::SM2Profile*>(terrainManager->GetTerrainGlobals()->getDefaultMaterialGenerator()->getActiveProfile());
	matProfile->setReceiveDynamicShadowsEnabled(enabled);
#ifdef SHADOWS_IN_LOW_LOD_MATERIAL
	matProfile->setReceiveDynamicShadowsLowLod(true);
#else
	matProfile->setReceiveDynamicShadowsLowLod(false);
#endif

	resetMaterials();

	if(enabled)
	{
		sceneManager->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_ADDITIVE_INTEGRATED);
		/** �ڴ˾����ϣ���ʾComposite map��������Ӱ
		*/
		sceneManager->setShadowFarDistance(12800.0f);

		sceneManager->setShadowTextureCountPerLightType(Ogre::Light::LT_DIRECTIONAL, 3);
		if(shadowCameraSetup.isNull())
		{
			Ogre::PSSMShadowCameraSetup* pssmSetup = new Ogre::PSSMShadowCameraSetup();
			pssmSetup->setSplitPadding(camera->getNearClipDistance());
			pssmSetup->calculateSplitPoints(3, camera->getNearClipDistance(), sceneManager->getShadowFarDistance());
			pssmSetup->setOptimalAdjustFactor(0, 2);
			pssmSetup->setOptimalAdjustFactor(1, 1);
			pssmSetup->setOptimalAdjustFactor(2, 0.5);

			shadowCameraSetup.bind(pssmSetup);

		}
		sceneManager->setShadowCameraSetup(shadowCameraSetup);

		if(depthShadows)
		{
			sceneManager->setShadowTextureCount(3);
			sceneManager->setShadowTextureConfig(0, 2048, 2048, Ogre::PF_FLOAT32_R);
			sceneManager->setShadowTextureConfig(1, 1024, 1024, Ogre::PF_FLOAT32_R);
			sceneManager->setShadowTextureConfig(2, 1024, 1024, Ogre::PF_FLOAT32_R);
			sceneManager->setShadowTextureSelfShadow(true);
			sceneManager->setShadowCasterRenderBackFaces(true);

			Ogre::Vector4 splitPoints;
			const Ogre::PSSMShadowCameraSetup::SplitPointList& splitPointList = 
				static_cast<Ogre::PSSMShadowCameraSetup*>(shadowCameraSetup.get())->getSplitPoints();
			for(int i = 0; i < 3; ++i)
			{
				splitPoints[i] = splitPointList[i];
			}
			Ogre::GpuSharedParametersPtr p = Ogre::GpuProgramManager::getSingleton().getSharedParameters("pssm_params");
			p->setNamedConstant("pssmSplitPoints", splitPoints);

			for(int i=0; i<objects.size(); i++)
			{
				buildDepthShadowMaterial(objects[i]);
			}
		}
		else
		{
			sceneManager->setShadowTextureCount(3);
			sceneManager->setShadowTextureConfig(0, 2048, 2048, Ogre::PF_X8B8G8R8);
			sceneManager->setShadowTextureConfig(1, 1024, 1024, Ogre::PF_X8B8G8R8);
			sceneManager->setShadowTextureConfig(2, 1024, 1024, Ogre::PF_X8B8G8R8);
			sceneManager->setShadowTextureSelfShadow(false);
			sceneManager->setShadowCasterRenderBackFaces(false);
			sceneManager->setShadowTextureCasterMaterial(Ogre::StringUtil::BLANK);
		}

		matProfile->setReceiveDynamicShadowsDepth(depthShadows);
		matProfile->setReceiveDynamicShadowsPSSM(static_cast<Ogre::PSSMShadowCameraSetup*>(shadowCameraSetup.get()));
	}
	else
	{
		sceneManager->setShadowTechnique(Ogre::SHADOWTYPE_NONE);
	}

	this->shadowTechnique = shadowTechnique;
}

BEGIN_MESSAGE_MAP(SceneDoc, CDocument)

	ON_COMMAND(ID_SAVE_SCENE, &SceneDoc::OnSaveScene)
	ON_COMMAND_RANGE(ID_TRANS_OBJECT, ID_LIQUID_EDIT, OnObjectEdit)
	ON_UPDATE_COMMAND_UI_RANGE(ID_TRANS_OBJECT, ID_LIQUID_EDIT, OnUpdateObjectEdit)
	ON_COMMAND(ID_ADD_LIGHT, OnAddLight)
	ON_UPDATE_COMMAND_UI(ID_ADD_LIGHT, OnUpdateAddLight)
	ON_COMMAND(ID_OBJECT_PASTE, OnObjectPaste)
	ON_UPDATE_COMMAND_UI(ID_OBJECT_PASTE, OnUpdateObjectPaste)
	ON_COMMAND(ID_SHOW_DEBUG_OVERLAY, OnShowDebugOverlay)
	ON_UPDATE_COMMAND_UI(ID_SHOW_DEBUG_OVERLAY, OnUpdateShowDebugOverlay)

	/** Technique
	*/

	// Deferred Shading
	ON_COMMAND(ID_DEFERREDSHADING_ACTIVE, &SceneDoc::OnDeferredshadingActive)
	ON_UPDATE_COMMAND_UI(ID_DEFERREDSHADING_ACTIVE, &SceneDoc::OnUpdateDeferredshadingActive)

	ON_COMMAND(ID_DEFERREDSHADING_REGULARVIEW, &SceneDoc::OnDeferredshadingRegularview)
	ON_COMMAND(ID_DEFERREDSHADING_DEBUGCOLOURS, &SceneDoc::OnDeferredshadingDebugcolours)
	ON_COMMAND(ID_DEFERREDSHADING_DEBUGNORMALS, &SceneDoc::OnDeferredshadingDebugnormals)
	ON_COMMAND(ID_DEFERREDSHADING_DEBUGDEPTH, &SceneDoc::OnDeferredshadingDebugdepth)
	ON_UPDATE_COMMAND_UI_RANGE(ID_DEFERREDSHADING_REGULARVIEW, ID_DEFERREDSHADING_DEBUGDEPTH, &SceneDoc::OnUpdateDeferredShading)
	
	ON_COMMAND(ID_SSAO, &SceneDoc::OnSsao)
	ON_UPDATE_COMMAND_UI(ID_SSAO, &SceneDoc::OnUpdateSsao)

	// ~
	ON_UPDATE_COMMAND_UI_RANGE(ID_LOAD_BRUSH, ID_RESIZE_BRUSH, OnUpdateBrushMenu)
	ON_UPDATE_COMMAND_UI_RANGE(ID_LOAD_TEXTURE, ID_RESIZE_TEXTURE, OnUpdateTextureMenu)

	ON_COMMAND(ID_DEFERREDSHADING_SHADOW, &SceneDoc::OnDeferredshadingShadow)
	ON_UPDATE_COMMAND_UI(ID_DEFERREDSHADING_SHADOW, &SceneDoc::OnUpdateDeferredshadingShadow)
	ON_COMMAND(ID_STATUSBAR_CAMERA_SPEED_SLIDER, &SceneDoc::OnStatusBarCameraSpeedSlider)
	ON_COMMAND(ID_PRINT_SCREEN, &SceneDoc::OnPrintScreen)
END_MESSAGE_MAP()

void SceneDoc::OnSaveScene()
{
	if(initialized)
	{
		terrainManager->GetTerrainGroup()->saveAllTerrains(false);

		TiXmlDocument *Doc = new TiXmlDocument;

		TiXmlDeclaration *Decl = new TiXmlDeclaration("1.0", "", "");
		Doc->LinkEndChild(Decl);

		TiXmlElement *Root = new TiXmlElement(sceneName), *Elmt = NULL;
		Doc->LinkEndChild(Root);

		char Buff[512];

		//////////////////////////////////////////////////
		// Ambient light
		//////////////////////////////////////////////////

		Ogre::ColourValue AmbientLight = sceneManager->getAmbientLight();
		sprintf_s(Buff, "%f,%f,%f", AmbientLight.r, AmbientLight.g, AmbientLight.b);
		Root->SetAttribute("AmbientLight", Buff);

		//////////////////////////////////////////////////
		// Fog
		//////////////////////////////////////////////////

		Elmt = new TiXmlElement("Fog");
		Root->LinkEndChild(Elmt);

		switch(sceneManager->getFogMode())
		{
		case Ogre::FOG_NONE:
			{
				Elmt->SetAttribute("Mode", "None");
			}
			break;

		case Ogre::FOG_LINEAR:
			{
				Elmt->SetAttribute("Mode", "Linear");
			}
			break;

		case Ogre::FOG_EXP:
			{
				Elmt->SetAttribute("Mode", "Exp");
			}
			break;

		case Ogre::FOG_EXP2:
			{
				Elmt->SetAttribute("Mode", "Exp2");
			}
		}

		Ogre::ColourValue FogColour = sceneManager->getFogColour();
		sprintf_s(Buff, "%f,%f,%f", FogColour.r, FogColour.g, FogColour.b);
		Elmt->SetAttribute("Colour", Buff);

		sprintf_s(Buff, "%f", sceneManager->getFogDensity());
		Elmt->SetAttribute("Density", Buff);

		sprintf_s(Buff, "%f", sceneManager->getFogStart());
		Elmt->SetAttribute("Start", Buff);

		sprintf_s(Buff, "%f", sceneManager->getFogEnd());
		Elmt->SetAttribute("End", Buff);

		//////////////////////////////////////////////////
		// Sky
		//////////////////////////////////////////////////

		Elmt = new TiXmlElement("Sky");
		Root->LinkEndChild(Elmt);

		Elmt->SetAttribute("Type", skyType);
		Elmt->SetAttribute("Material", skyMaterial);

		//////////////////////////////////////////////////
		// Terrain manager config
		//////////////////////////////////////////////////

		Elmt = new TiXmlElement("TerrainManagerConfig");
		Root->LinkEndChild(Elmt);

		TiXmlElement *SubElmt = NULL;
		
		SubElmt = new TiXmlElement("LightMap");
		Elmt->LinkEndChild(SubElmt);

		sprintf_s(Buff, "%f,%f,%f", terrainManagerConfig->LightMapDirection.x,
			terrainManagerConfig->LightMapDirection.y, terrainManagerConfig->LightMapDirection.z);
		SubElmt->SetAttribute("Direction", Buff);

		sprintf_s(Buff, "%u", terrainManagerConfig->LightMapSize);
		SubElmt->SetAttribute("Size", Buff);

		SubElmt = new TiXmlElement("CompisiteMap");
		Elmt->LinkEndChild(SubElmt);

		sprintf_s(Buff, "%f,%f,%f", terrainManagerConfig->CompositeMapAmbient.r,
			terrainManagerConfig->CompositeMapAmbient.g, terrainManagerConfig->CompositeMapAmbient.b);
		SubElmt->SetAttribute("Ambient", Buff);

		sprintf_s(Buff, "%f,%f,%f", terrainManagerConfig->CompositeMapDiffuse.r,
			terrainManagerConfig->CompositeMapDiffuse.g, terrainManagerConfig->CompositeMapDiffuse.b);
		SubElmt->SetAttribute("Diffuse", Buff);

		sprintf_s(Buff, "%f", terrainManagerConfig->CompositeMapDistance);
		SubElmt->SetAttribute("Distance", Buff);

		sprintf_s(Buff, "%u", terrainManagerConfig->CompositeMapSize);
		SubElmt->SetAttribute("Size", Buff);

		SubElmt = new TiXmlElement("TerrainGroup");
		Elmt->LinkEndChild(SubElmt);

		SubElmt->SetAttribute("FilenamePrefix", terrainManagerConfig->FilenamePrefix);
		SubElmt->SetAttribute("FilenameExtension", terrainManagerConfig->FilenameExtension);
		sprintf_s(Buff, "%f", terrainManagerConfig->TerrainSize);
		SubElmt->SetAttribute("TerrainSize", Buff);
		sprintf_s(Buff, "%f", terrainManagerConfig->WorldSize);
		SubElmt->SetAttribute("WorldSize", Buff);
		sprintf_s(Buff, "%f", terrainManagerConfig->InputScale);
		SubElmt->SetAttribute("InputScale", Buff);
		sprintf_s(Buff, "%f", terrainManagerConfig->MinBatchSize);
		SubElmt->SetAttribute("MinBatchSize", Buff);
		sprintf_s(Buff, "%f", terrainManagerConfig->MaxBatchSize);
		SubElmt->SetAttribute("MaxBatchSize", Buff);

		Ogre::Terrain *Terrain = terrainManager->GetTerrainGroup()->getTerrain(0, 0);
		for(int i=0; i<Terrain->getLayerCount(); i++)
		{
			SubElmt = new TiXmlElement("Layer");
			Elmt->LinkEndChild(SubElmt);

			SubElmt->SetAttribute("Index", 0);
			sprintf_s(Buff, "%f", Terrain->getLayerWorldSize(i));
			SubElmt->SetAttribute("TextureWorldSize", Buff);
			SubElmt->SetAttribute("DiffuseSpecular", Terrain->getLayerTextureName(i, 0));
			SubElmt->SetAttribute("NormalHeight", Terrain->getLayerTextureName(i, 1));
		}

		Elmt = new TiXmlElement("Objects");
		Root->LinkEndChild(Elmt);

		for(int i=0; i<objects.size(); i++)
		{
			switch(objects[i]->getType())
			{
			case SceneObject::STATIC_MESH:
				{
					SubElmt = new TiXmlElement("StaticMesh");
					Elmt->LinkEndChild(SubElmt);

					StaticMesh *Object = (StaticMesh*)objects[i];
					SubElmt->SetAttribute("Path", Object->getPath());

					const Ogre::Vector3 &Position = Object->getSceneNode()->getPosition();
					sprintf_s(Buff, "%f,%f,%f", Position.x, Position.y, Position.z);
					SubElmt->SetAttribute("Position", Buff);

					const Ogre::Quaternion &Direction = Object->getSceneNode()->getOrientation();
					sprintf_s(Buff, "%f,%f,%f,%f", 
						Direction.w,
						Direction.x,
						Direction.y, 
						Direction.z);
					SubElmt->SetAttribute("Direction", Buff);

					const Ogre::Vector3 &Scale = Object->getSceneNode()->getScale();
					sprintf_s(Buff, "%f,%f,%f", Scale.x, Scale.y, Scale.z);
					SubElmt->SetAttribute("Scale", Buff);
				}
				break;

			case SceneObject::DYNAMIC_MODEL:
				{
					SubElmt = new TiXmlElement("DynamicModel");
					Elmt->LinkEndChild(SubElmt);

					DynamicModel *Object = (DynamicModel*)objects[i];
					SubElmt->SetAttribute("Path", Object->getPath());

					const Ogre::Vector3 &Position = Object->getSceneNode()->getPosition();
					sprintf_s(Buff, "%f,%f,%f", Position.x, Position.y, Position.z);
					SubElmt->SetAttribute("Position", Buff);

					const Ogre::Quaternion &Direction = Object->getSceneNode()->getOrientation();
					sprintf_s(Buff, "%f,%f,%f,%f", 
						Direction.w,
						Direction.x,
						Direction.y, 
						Direction.z);
					SubElmt->SetAttribute("Direction", Buff);

					const Ogre::Vector3 &Scale = Object->getSceneNode()->getScale();
					sprintf_s(Buff, "%f,%f,%f", Scale.x, Scale.y, Scale.z);
					SubElmt->SetAttribute("Scale", Buff);
				}
				break;

			case SceneObject::LIGHT:
				{
					SubElmt = new TiXmlElement("Light");
					Elmt->LinkEndChild(SubElmt);

					Light *Object = (Light*)objects[i];
					switch(Object->getLight()->getType())
					{
					case Ogre::Light::LT_POINT:
						{
							SubElmt->SetAttribute("Type", "Point");
						}
						break;

					case Ogre::Light::LT_DIRECTIONAL:
						{
							SubElmt->SetAttribute("Type", "Directional");
						}
						break;

					case Ogre::Light::LT_SPOTLIGHT:
						{
							SubElmt->SetAttribute("Type", "Spotlight");
						}
					}

					Ogre::ColourValue DiffuseColour = Object->getLight()->getDiffuseColour();
					sprintf_s(Buff, "%f,%f,%f", DiffuseColour.r, DiffuseColour.g, DiffuseColour.b);
					SubElmt->SetAttribute("DiffuseColour", Buff);

					Ogre::ColourValue SpecularColour = Object->getLight()->getSpecularColour();
					sprintf_s(Buff, "%f,%f,%f", SpecularColour.r, SpecularColour.g, SpecularColour.b);
					SubElmt->SetAttribute("SpecularColour", Buff);

					const Ogre::Vector3 &Position = Object->getSceneNode()->getPosition();
					sprintf_s(Buff, "%f,%f,%f", Position.x, Position.y, Position.z);
					SubElmt->SetAttribute("Position", Buff);

					const Ogre::Vector3 &Direction = Object->getLight()->getDirection();
					sprintf_s(Buff, "%f,%f,%f", Direction.x, Direction.y, Direction.z);
					SubElmt->SetAttribute("Direction", Buff);

					sprintf_s(Buff, "%f,%f,%f,%f", 
						Object->getLight()->getAttenuationRange(),
						Object->getLight()->getAttenuationConstant(),
						Object->getLight()->getAttenuationLinear(),
						Object->getLight()->getAttenuationQuadric());
					SubElmt->SetAttribute("Attenuation", Buff);

					sprintf_s(Buff, "%f,%f,%f", 
						Object->getLight()->getSpotlightInnerAngle().valueRadians(),
						Object->getLight()->getSpotlightOuterAngle().valueRadians(),
						Object->getLight()->getSpotlightFalloff());
					SubElmt->SetAttribute("SpotlightRange", Buff);
				}
			}
		}

		Doc->SaveFile("../../TestMedia/Scene/" + sceneName + ".scene");
	}
}

void SceneDoc::OnObjectEdit(UINT id)
{
	if(editMode == id)
	{
		switch(id)
		{
		case ID_TRANS_OBJECT:
		case ID_SCALE_OBJECT:
		case ID_ROTATE_OBJECT:
		case ID_DEL_OBJECT:
			{
				objectEditHandler->SetMode(OEM_NONE);
			}
			break;

		case ID_SELECT_TERRAIN:
		case ID_DEFORM_TERRAIN:
		case ID_BLEND_TERRAIN:
			{
				terrainEditHandler->SetMode(TEM_NONE);
			}
			break;

		case ID_LIQUID_EDIT:
			{
				liquidEditHandler->EndEdit();
			}
		}
		editMode = 0;
	}
	else
	{
		switch(id)
		{
		case ID_TRANS_OBJECT:
		case ID_SCALE_OBJECT:
		case ID_ROTATE_OBJECT:
		case ID_DEL_OBJECT:
			{
				objectEditHandler->SetMode(KObjectEditMode(id));
				terrainEditHandler->SetMode(TEM_NONE);
				liquidEditHandler->EndEdit();
			}
			break;

		case ID_SELECT_TERRAIN:
		case ID_DEFORM_TERRAIN:
		case ID_BLEND_TERRAIN:
			{
				terrainEditHandler->SetMode(KTerrainEditMode(id));
				objectEditHandler->SetMode(OEM_NONE);
				liquidEditHandler->EndEdit();
			}
			break;

		case ID_LIQUID_EDIT:
			{
				liquidEditHandler->BeginEdit();
				terrainEditHandler->SetMode(TEM_NONE);
				objectEditHandler->SetMode(OEM_NONE);
			}
		}
		editMode = id;
	}
}

void SceneDoc::OnUpdateObjectEdit(CCmdUI* pCmdUI)
{
	if(!initialized)
	{
		pCmdUI->Enable(FALSE);
		return;
	}
	pCmdUI->SetCheck(pCmdUI->m_nID == editMode);
}

void SceneDoc::OnAddLight()
{
	NewLightDlg Dlg;
	if(Dlg.DoModal() == IDOK)
	{
		Light *light = addLight();

		CString Type = Dlg.getProperty(NewLightDlg::TYPE);
		if(Type == "Point")
		{
			light->getLight()->setType(Ogre::Light::LT_POINT);
		} else 
		if(Type == "Directional")
		{
			light->getLight()->setType(Ogre::Light::LT_DIRECTIONAL);
		} else 
		if(Type == "Spotlight")
		{
			light->getLight()->setType(Ogre::Light::LT_SPOTLIGHT);
		}

		COLORREF refDiffuseColour = Dlg.getProperty(NewLightDlg::DIFFUSE_COLOUR);
		light->getLight()->setDiffuseColour(
			Ogre::ColourValue(GetRValue(refDiffuseColour)/255.0f, GetGValue(refDiffuseColour)/255.0f, GetBValue(refDiffuseColour)/255.0f));

		COLORREF refSpecularColour = Dlg.getProperty(NewLightDlg::SPECULAR_COLOUR);
		light->getLight()->setSpecularColour(
			Ogre::ColourValue(GetRValue(refSpecularColour)/255.0f, GetGValue(refSpecularColour)/255.0f, GetBValue(refSpecularColour)/255.0f));

		float PositionX = Dlg.getProperty(NewLightDlg::POSITION_X);
		float PositionY = Dlg.getProperty(NewLightDlg::POSITION_Y);
		float PositionZ = Dlg.getProperty(NewLightDlg::POSITION_Z);
		light->getSceneNode()->setPosition(Ogre::Vector3(PositionX, PositionY, PositionZ));

		float DirectionX = Dlg.getProperty(NewLightDlg::DIRECTION_X);
		float DirectionY = Dlg.getProperty(NewLightDlg::DIRECTION_Y);
		float DirectionZ = Dlg.getProperty(NewLightDlg::DIRECTION_Z);
		light->getLight()->setDirection(Ogre::Vector3(DirectionX, DirectionY, DirectionZ));

		float AttenuationRange = Dlg.getProperty(NewLightDlg::ATTENUATION_RANGE);
		float AttenuationConstant = Dlg.getProperty(NewLightDlg::ATTENUATION_CONSTANT);
		float AttenuationLinear = Dlg.getProperty(NewLightDlg::ATTENUATION_LINEAR);
		float AttenuationQuadratic = Dlg.getProperty(NewLightDlg::ATTENUATION_QUADRATIC);
		light->getLight()->setAttenuation(AttenuationRange, AttenuationConstant, AttenuationLinear, AttenuationQuadratic);

		float SpotlightInnerAngle = Dlg.getProperty(NewLightDlg::SPOTLIGHT_RANGE_INNER_ANGLE);
		float SpotlightOuterAngle = Dlg.getProperty(NewLightDlg::SPOTLIGHT_RANGE_OUTER_ANGLE);
		float SpotlightFalloff = Dlg.getProperty(NewLightDlg::SPOTLIGHT_RANGE_FALLOFF);
		light->getLight()->setSpotlightRange(Ogre::Radian(SpotlightInnerAngle), Ogre::Radian(SpotlightOuterAngle), SpotlightFalloff);

		light->showIndicator();
	}
}

void SceneDoc::OnUpdateAddLight(CCmdUI *pCmdUI)
{
}

void SceneDoc::OnObjectPaste()
{
	paste = !paste;
}

void SceneDoc::OnUpdateObjectPaste(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(paste);
}

void SceneDoc::OnShowDebugOverlay()
{
	showDebugOverlay = !showDebugOverlay;
	RenderPump::current->showDebugOverlay(showDebugOverlay);
}

void SceneDoc::OnUpdateShowDebugOverlay(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(showDebugOverlay);
}

void SceneDoc::OnDeferredshadingActive()
{
	active = !active;
	deferredShadingSystem->setActive(active);
}

void SceneDoc::OnUpdateDeferredshadingActive(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(active);
}

void SceneDoc::OnDeferredshadingRegularview()
{
	deferredShadingSystem->setMode(DeferredShadingSystem::DSM_SHOWLIT);
	deferredShadingMode = ID_DEFERREDSHADING_REGULARVIEW;
}

void SceneDoc::OnDeferredshadingDebugcolours()
{
	deferredShadingSystem->setMode(DeferredShadingSystem::DSM_SHOWCOLOUR);
	deferredShadingMode = ID_DEFERREDSHADING_DEBUGCOLOURS;
}

void SceneDoc::OnDeferredshadingDebugnormals()
{
	deferredShadingSystem->setMode(DeferredShadingSystem::DSM_SHOWNORMALS);
	deferredShadingMode = ID_DEFERREDSHADING_DEBUGNORMALS;
}

void SceneDoc::OnDeferredshadingDebugdepth()
{
	deferredShadingSystem->setMode(DeferredShadingSystem::DSM_SHOWDSP);
	deferredShadingMode = ID_DEFERREDSHADING_DEBUGDEPTH;
}

void SceneDoc::OnUpdateDeferredShading(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(pCmdUI->m_nID == deferredShadingMode);
}

void SceneDoc::OnSsao()
{
	ssao = !ssao;
	deferredShadingSystem->setSSAO(ssao);
}

void SceneDoc::OnUpdateSsao(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(ssao);
}

void SceneDoc::OnUpdateBrushMenu(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
}

void SceneDoc::OnUpdateTextureMenu(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
}

void SceneDoc::OnDeferredshadingShadow()
{
	shadow = !shadow;
	sceneManager->setShadowTechnique(shadow ? Ogre::SHADOWTYPE_TEXTURE_ADDITIVE : Ogre::SHADOWTYPE_NONE);
}

void SceneDoc::OnUpdateDeferredshadingShadow(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(shadow);
}

void SceneDoc::OnStatusBarCameraSpeedSlider()
{
	CBCGPRibbonSlider *cameraSpeedSlider =
		(CBCGPRibbonSlider*)(((MainFrame*)AfxGetMainWnd())->getStatusBar()->FindElement(ID_STATUSBAR_CAMERA_SPEED_SLIDER));
	cameraManager.setSpeed(cameraSpeedSlider->GetPos() * 10.0f);
}

void SceneDoc::resetMaterials()
{
	for(int i=0; i<objects.size(); i++)
	{
		Ogre::Entity *entity = objects[i]->getEntity();
		if(entity != NULL)
		{
			std::map<Ogre::SubEntity*, std::string> &nativeMaterials = 
				objects[i]->getNativeMaterials();
			for(int j=0; j<entity->getNumSubEntities(); j++)
			{
				Ogre::SubEntity *subEntity = entity->getSubEntity(i);
				subEntity->setMaterialName(nativeMaterials[subEntity]);
			}
		}
	}
}

void SceneDoc::OnPrintScreen()
{
	activeView->getRenderWindow()->writeContentsToTimestampedFile("PrintScreen", ".png");
	AfxMessageBox("�ɹ�");
}
