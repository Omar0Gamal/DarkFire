#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

static void ReplaceAllInFile(const fs::path& filePath, const std::string& from, const std::string& to)
{
	if (!fs::is_regular_file(filePath))
	{
		return;
	}

	std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
	if (!inFile.is_open())
	{
		return;
	}

	std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
	inFile.close();

	size_t pos = 0;
	bool changed = false;
	while ((pos = content.find(from, pos)) != std::string::npos)
	{
		content.replace(pos, from.length(), to);
		pos += to.length();
		changed = true;
	}

	if (changed)
	{
		std::ofstream outFile(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (outFile.is_open())
		{
			outFile << content;
			outFile.close();
		}
	}
}

int main(int argc, char** argv)
{
	std::string projectName;
	std::string templateName = "Empty3D";
	std::string targetPath;

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "--template" && i + 1 < argc)
		{
			templateName = argv[++i];
		}
		else if (arg == "--path" && i + 1 < argc)
		{
			targetPath = argv[++i];
		}
		else if (projectName.empty() && arg.rfind("--", 0) != 0)
		{
			projectName = arg;
		}
	}

	if (projectName.empty())
	{
		std::cout << "Usage: dfnew <ProjectName> [--template <Empty3D|Playground3D|GraphicsTest3D>] [--path <DestinationPath>]\n";
		return 1;
	}

	if (targetPath.empty())
	{
		targetPath = projectName;
	}

	fs::path templateDir = fs::path("Templates") / templateName;
	if (!fs::exists(templateDir))
	{
		std::cerr << "[dfnew] Error: Template not found at '" << templateDir.string() << "'\n";
		return 1;
	}

	fs::path destDir = targetPath;
	if (fs::exists(destDir) && !fs::is_empty(destDir))
	{
		std::cerr << "[dfnew] Error: Target directory '" << destDir.string() << "' already exists and is not empty!\n";
		return 1;
	}

	std::cout << "[dfnew] Creating project '" << projectName << "' from template '" << templateName << "'...\n";
	fs::create_directories(destDir);

	// Copy recursively
	for (const auto& entry : fs::recursive_directory_iterator(templateDir))
	{
		fs::path relPath = fs::relative(entry.path(), templateDir);
		fs::path targetFilePath = destDir / relPath;

		if (entry.is_directory())
		{
			fs::create_directories(targetFilePath);
		}
		else if (entry.is_regular_file())
		{
			fs::create_directories(targetFilePath.parent_path());
			fs::copy_file(entry.path(), targetFilePath, fs::copy_options::overwrite_existing);

			// Rename if file contains templateName in its filename
			std::string oldFilename = targetFilePath.filename().string();
			size_t namePos = oldFilename.find(templateName);
			if (namePos != std::string::npos)
			{
				std::string newFilename = oldFilename;
				newFilename.replace(namePos, templateName.length(), projectName);
				fs::path renamedPath = targetFilePath.parent_path() / newFilename;
				fs::rename(targetFilePath, renamedPath);
				targetFilePath = renamedPath;
			}

			// Replace template occurrences in text file contents
			std::string ext = targetFilePath.extension().string();
			if (ext == ".dfproj" || ext == ".cpp" || ext == ".h" || ext == ".lua" || ext == ".scene" || ext == ".vcxproj" || ext == ".filters")
			{
				ReplaceAllInFile(targetFilePath, templateName, projectName);
			}
		}
	}

	std::cout << "[dfnew] Successfully generated project '" << projectName << "' at '" << destDir.string() << "'!\n";
	return 0;
}
