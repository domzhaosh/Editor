#include "stdafx.h"
#include "Light.h"

#include "OgreSceneManager.h"
#include "OgreEntity.h"

Light::Light(Ogre::SceneManager *sceneManager):
	SceneObject(sceneManager, SceneObject::LIGHT)
{
	light = NULL;
}

void Light::create(std::string name, std::string path)
{
	SceneObject::create(name, path);
	light = sceneManager->createLight(name);
	light->setUserAny(Ogre::Any((SceneObject*)this));
	sceneNode->createChildSceneNode()->attachObject(light);
}

void Light::destroy()
{
	if(light != NULL)
		sceneManager->destroyLight(light);
	SceneObject::destroy();
}

Ogre::Light *Light::getLight()
{
	return light;
}

void Light::showIndicator()
{
	Ogre::Entity* arrow = sceneManager->createEntity(light->getName() + "_Indicator", "pointer100m.mesh");
	arrow->setCastShadows(false);
	arrow->setMaterialName("BaseWhite");
	arrow->setUserAny(Ogre::Any((SceneObject*)this));
	sceneNode->createChildSceneNode()->attachObject(arrow);
	if(light->getType() == Ogre::Light::LT_DIRECTIONAL)
	{
		const Ogre::Vector3 &direction = light->getDirection();
		arrow->getParentSceneNode()->setDirection(direction, Ogre::Node::TS_LOCAL, Ogre::Vector3::UNIT_Y);
	}
}
