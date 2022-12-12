#include "ObjManager.h"

ObjManager ObjManager::instance;
Loader ObjManager::loader;

ObjManager::ObjManager()
{

}

ObjManager::~ObjManager()
{

}

ObjManager* ObjManager::GetInstance()
{
	return &instance;
}

bool ObjManager::LoadFile(std::string& Path)
{
	return loader.LoadFile(Path);
}

Loader* ObjManager::GetLoader()
{
	return &loader;
}
