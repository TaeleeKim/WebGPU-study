#pragma once
#ifndef _OBJMANAGER_H_
#define _OBJMANAGER_H_
#include <string>

#define ID_IMPORT_OBJ 1004

#include "ObjLoader.h"
class ObjManager
{
private:
	ObjManager();
	ObjManager(const ObjManager& objLoader);
	virtual ~ObjManager();

public:
	static ObjManager* GetInstance();

	bool LoadFile(std::string& Path);
	Loader* GetLoader();


private:
	static ObjManager instance;
	static Loader loader;
};


#endif /* _OBJMANAGER_H_ */
