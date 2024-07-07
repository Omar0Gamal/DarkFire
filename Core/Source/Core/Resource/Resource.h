#pragma once
#include <string>
namespace Core
{
	class ResourceManager;
	static std::wstring GetFileNameFromPath(const wchar_t* path);
	class Resource
	{
	public:
		Resource(const wchar_t* full_path, ResourceManager* manager);
		virtual ~Resource();

		std::wstring getPath();
		std::wstring getName();
	protected:
		std::wstring m_full_path;
		ResourceManager* m_resManager = nullptr;
	};
}

