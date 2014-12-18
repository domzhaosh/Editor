#pragma once

#include "DeferredShading.h"
#include "OgreShadowCameraSetup.h"
#include "RenderScene.h"
#include <vector>

class DynamicModel;
class Light;
class LiquidEditHandler;
class NewLightDlg;
class NewSceneDlg;
class ObjectEditHandler;
class SceneObject;
class SceneView;
class StaticMesh;
class TerrainEditHandler;
class TerrainManager;
class TerrainManagerConfig;

class SceneDoc : public RenderScene
{
	DECLARE_DYNCREATE(SceneDoc)
public:
	SceneDoc();
	virtual ~SceneDoc();

	bool isPaste() { return paste; }

	CString getSceneName() { return sceneName; }
	void setSceneName(CString sceneName) { SetTitle(sceneName); this->sceneName = sceneName; }

	CString getSkyType() { return skyType; }
	CString getSkyMaterial() { return skyMaterial; }

	SceneView *getActiveView() { return activeView; }
	void setActiveView(SceneView *activeView) { this->activeView = activeView; }

	DeferredShadingSystem *getDeferredShadingSystem() { return deferredShadingSystem; }
	void setDeferredShadingSystem(DeferredShadingSystem *deferredShadingSystem) { this->deferredShadingSystem = deferredShadingSystem; }

	TerrainManager *getTerrainManager() { return terrainManager; }
	TerrainManagerConfig *getTerrainManagerConfig() { return terrainManagerConfig; }

	TerrainEditHandler *getTerrainEditHandler() { return terrainEditHandler; }
	ObjectEditHandler *getObjectEditHandler() { return objectEditHandler; }
	LiquidEditHandler *getLiquidEditHandler() { return liquidEditHandler; }

	int getNameID() { return nameID; }
	void setNameID(int nameID) { this->nameID = nameID; }

	std::vector<SceneObject*> &getObjects() { return objects; }

	/** ��ֹ��δ��ʼ��������µ���SceneResourceTree������
	*/
	bool isInitialized() { return initialized; }
	void initialize(NewSceneDlg *Dlg); void initialize(CString Filename);
	void destroy();

	StaticMesh *addStaticMesh(CString path);
	DynamicModel *addDynamicModel(CString path);
	Light *addLight();

	void selectObject(SceneObject *object);
	void removeObject(SceneObject *object);
	void configureShadows(bool enabled, bool depthShadows);

	void roaming(CPoint point, float elapsed);
	void leftDown(UINT nFlags, CPoint point);
	void leftUp(UINT nFlags, CPoint point);

	void update(float elapsed);

	static SceneDoc *current;

	afx_msg void OnSaveScene();
	afx_msg void OnObjectEdit(UINT id);
	afx_msg void OnUpdateObjectEdit(CCmdUI* pCmdUI);
	afx_msg void OnAddLight();
	afx_msg void OnUpdateAddLight(CCmdUI *pCmdUI);
	afx_msg void OnObjectPaste();
	afx_msg void OnUpdateObjectPaste(CCmdUI *pCmdUI);
	afx_msg void OnShowDebugOverlay();
	afx_msg void OnUpdateShowDebugOverlay(CCmdUI *pCmdUI);
	
	/** Technique
	*/

	// Shadow
	afx_msg void OnShadowTechniqueNone();
	afx_msg void OnShadowTechniqueStencil();
	afx_msg void OnShadowTechniqueTexture();
	afx_msg void OnUpdateShadowTechnique(CCmdUI *pCmdUI);
	afx_msg void OnShadowLightingAdditive();
	afx_msg void OnShadowLightingModulative();
	afx_msg void OnUpdateShadowLighting(CCmdUI *pCmdUI);
	afx_msg void OnShadowProjectionUniform();
	afx_msg void OnShadowProjectionUniformfocused();
	afx_msg void OnShadowProjectionLispsm();
	afx_msg void OnShadowProjectionPlaneoptimal();
	afx_msg void OnUpdateShadowProjection(CCmdUI *pCmdUI);
	afx_msg void OnShadowMaterialStandard();
	afx_msg void OnShadowMaterialDepthshadowmap();
	afx_msg void OnShadowMaterialDepthshadowmapPcf();
	afx_msg void OnUpdateShadowMaterial(CCmdUI *pCmdUI);

	// Deferred Shading
	afx_msg void OnDeferredshadingActive();
	afx_msg void OnUpdateDeferredshadingActive(CCmdUI *pCmdUI);
	afx_msg void OnDeferredshadingRegularview();
	afx_msg void OnDeferredshadingDebugcolours();
	afx_msg void OnDeferredshadingDebugnormals();
	afx_msg void OnDeferredshadingDebugdepth();
	afx_msg void OnUpdateDeferredShading(CCmdUI *pCmdUI);
	afx_msg void OnSsao();
	afx_msg void OnUpdateSsao(CCmdUI *pCmdUI);
	
	// ~
	afx_msg void OnUpdateBrushMenu(CCmdUI* pCmdUI);
	afx_msg void OnUpdateTextureMenu(CCmdUI* pCmdUI);

	DECLARE_MESSAGE_MAP()

	void handleShadowTypeChanged();
	void handleProjectionChanged();
	void handleMaterialChanged();
	void resetMaterials();

	int editMode;
	bool paste; bool showDebugOverlay;

	bool initialized;
	CString sceneName;
	CString skyType; CString skyMaterial;

	// Shadow
	int shadowTechnique;
	int shadowLighting;
	int shadowProjection;
	int shadowMaterial;

	// DeferredShading
	DeferredShadingSystem *deferredShadingSystem;
	bool active, shadow, ssao;
	int deferredShadingMode;

	SceneView *activeView;
	TerrainManager *terrainManager;
	TerrainManagerConfig *terrainManagerConfig;
	Ogre::ShadowCameraSetupPtr shadowCameraSetup;

	TerrainEditHandler *terrainEditHandler;
	ObjectEditHandler *objectEditHandler;
	LiquidEditHandler *liquidEditHandler;

	int nameID;
	std::vector<SceneObject*> objects;
	afx_msg void OnDeferredshadingShadow();
	afx_msg void OnUpdateDeferredshadingShadow(CCmdUI *pCmdUI);
};
