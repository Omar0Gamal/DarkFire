#include "Resource.h"
#include "ResourceManager.h"
#include <filesystem>

namespace Core
{
	Resource::Resource(const wchar_t* full_path, ResourceManager* manager) 
		: m_full_path(full_path), m_resManager(manager){}


	Resource::~Resource() {}

	std::wstring Resource::getPath()
	{
		return m_full_path;
	}
}
