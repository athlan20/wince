#include "XUtilsFile.h"
#include "../base/macros.h"
#include "../base/macros.h"
#include "../base/global.h"
#include "../utils/XUtilsFormatter.h"
#include "../utils/XUtilsFormatter.h"

//#define ACCESS _access
#define MKDIR(a) _mkdir((a))

#include <boost/unordered_map.hpp>
#include <windows.h>
#include <stdio.h>
#include <fstream>
#include <sstream>

#define X_MAX_PATH 512

boost::unordered_map<std::string, std::string> XUtilsFile::_fullPathCache;
boost::unordered_map<std::string, std::string> XUtilsFile::_filenameLookupDict;
std::vector<std::string> XUtilsFile::_searchPathArray;

// The root path of resources, the character encoding is UTF-8.
// UTF-8 is the only encoding supported by cocos2d-x API.
static std::string s_resourcePath = "";

std::string XUtilsFile::_defaultResRootPath = "";

#ifdef WIN32

BOOL WStringToString(const std::wstring &wstr, std::string &str)
{
	int nLen = (int)wstr.length();
	str.resize(nLen, ' ');

	int nResult = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)wstr.c_str(), nLen, (LPSTR)str.c_str(), nLen, NULL, NULL);

	if (nResult == 0)
	{
		return FALSE;
	}

	return TRUE;
}

#endif

// D:\aaa\bbb\ccc\ddd\abc.txt --> D:/aaa/bbb/ccc/ddd/abc.txt
static inline std::string convertPathFormatToUnixStyle(const std::string& path)
{
	std::string ret = path;
	int len = ret.length();
	for (int i = 0; i < len; ++i)
	{
		if (ret[i] == '\\')
		{
			ret[i] = '/';
		}
	}
	return ret;
}

static inline void _checkPath()
{
	if (0 == s_resourcePath.length())
	{
		WCHAR utf16Path[X_MAX_PATH] = { 0 };
//		GetCurrentDirectoryW(sizeof(utf16Path)-1, utf16Path);

		char utf8Path[X_MAX_PATH] = { 0 };
		int nNum = WideCharToMultiByte(CP_UTF8, 0, utf16Path, -1, utf8Path, sizeof(utf8Path), NULL, NULL);

		s_resourcePath = convertPathFormatToUnixStyle(utf8Path);
		s_resourcePath.append("/");
	}
}

void XUtilsFile::init()
{
	_checkPath();
	_defaultResRootPath = s_resourcePath;
	_searchPathArray.push_back(_defaultResRootPath);
}

/************************************************************************/
/* 查看文件是否存在                                                     */
/************************************************************************/
bool XUtilsFile::isFileExistInternal(const std::string& strFilePath)
{
	if (0 == strFilePath.length())
	{
		return false;
	}

	std::string strPath = strFilePath;
	if (!XUtilsFile::isAbsolutePath(strPath))
	{
		std::string defaultResRootPath = "";
		WCHAR utf16Path[X_MAX_PATH] = { 0 };
//		GetCurrentDirectoryW(sizeof(utf16Path)-1, utf16Path);
		char utf8Path[X_MAX_PATH] = { 0 };
		int nNum = WideCharToMultiByte(CP_UTF8, 0, utf16Path, -1, utf8Path, sizeof(utf8Path), NULL, NULL);
		defaultResRootPath = convertPathFormatToUnixStyle(utf8Path);
		// Not absolute path, add the default root path at the beginning.
		strPath.insert(0, defaultResRootPath);
	}

	WCHAR utf16Buf[X_MAX_PATH] = { 0 };
	MultiByteToWideChar(CP_UTF8, 0, strPath.c_str(), -1, utf16Buf, sizeof(utf16Buf) / sizeof(utf16Buf[0]));

	DWORD attr = GetFileAttributesW(utf16Buf);

	if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
		return false;   //  not a file
	return true;
}

bool XUtilsFile::isDirectoryExistInternal(const std::string& dirPath)
{
	
	unsigned long fAttrib = GetFileAttributes(XUtilsFormatter::MB2WC(dirPath.c_str()).c_str());
	if (fAttrib != INVALID_FILE_ATTRIBUTES &&
		(fAttrib & FILE_ATTRIBUTE_DIRECTORY))
	{
		return true;
	}
	return false;

}

std::string XUtilsFile::getNewFilename(const std::string &filename)
{
	std::string newFileName;

	// in Lookup Filename dictionary ?
	boost::unordered_map<std::string, std::string>::iterator iter = _filenameLookupDict.find(filename);

	if (iter == _filenameLookupDict.end())
	{
		newFileName = filename;
	}
	else
	{
		newFileName = iter->second;
	}
	return newFileName;
}


std::string XUtilsFile::getFullPathForDirectoryAndFilename(const std::string& directory, const std::string& filename)
{
	// get directory+filename, safely adding '/' as necessary 
	std::string ret = directory;
	if (directory.size() && directory[directory.size() - 1] != '/'){
		ret += '/';
	}
	ret += filename;

	// if the file doesn't exist, return an empty string
	if (!isFileExistInternal(ret)) {
		ret = "";
	}
	return ret;
}

std::string XUtilsFile::getPathForFilename(const std::string& filename, const std::string& searchPath)
{
	std::string file = filename;
	std::string file_path = "";
	size_t pos = filename.find_last_of("/");
	if (pos != std::string::npos)
	{
		file_path = filename.substr(0, pos + 1);
		file = filename.substr(pos + 1);
	}

	// searchPath + file_path + resourceDirectory
	std::string path = searchPath;
	path += file_path;

	path = getFullPathForDirectoryAndFilename(path, file);

	//CCLOG("getPathForFilename, fullPath = %s", path.c_str());
	return path;
}

std::string XUtilsFile::fullPathForFilename(const std::string &filename)
{
	if (filename.empty())
	{
		return "";
	}

	if (isAbsolutePath(filename))
	{
		return filename;
	}

	// Already Cached ?
	boost::unordered_map<std::string, std::string>::iterator cacheIter = _fullPathCache.find(filename);
	if (cacheIter != _fullPathCache.end())
	{
		return cacheIter->second;
	}

	// Get the new file name.
	const std::string newFilename(getNewFilename(filename));

	std::string fullpath;

	for (std::vector<std::string>::iterator searchIt = _searchPathArray.begin(); searchIt != _searchPathArray.end(); ++searchIt)
	{
		fullpath = getPathForFilename(newFilename, *searchIt);

		if (fullpath.length() > 0)
		{
			// Using the filename passed in as key.
			_fullPathCache.insert(std::make_pair(filename, fullpath));
			return fullpath;
		}
	}

	//if (isPopupNotify()){
	//	CCLOG("cocos2d: fullPathForFilename: No file found at %s. Possible missing file.", filename.c_str());
	//}

	// FIXME: Should it return nullptr ? or an empty string ?
	// The file wasn't found, return the file name passed in.
	return filename;
}

std::string XUtilsFile::searchFullPathForFilename(const std::string& filename)
{
	if (isAbsolutePath(filename))
	{
		return filename;
	}
	std::string path = XUtilsFile::fullPathForFilename(filename);
	//std::string path = const_cast<XUtilsFile*>(this)->fullPathForFilename(filename);
	if (0 == path.compare(filename))
	{
		return "";
	}
	else
	{
		return path;
	}
}

bool XUtilsFile::isAbsolutePath(const std::string& path)
{
#ifdef WIN32
	if (path.length() > 2
		&& ((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z'))
		&& path[1] == ':')
	{
		return true;
	}
	return false;
#else
	return (path[0] == '/');
#endif // WIN32
	
}

bool XUtilsFile::isFileExist(const std::string& filename)
{
	if (isAbsolutePath(filename))
	{
		return isFileExistInternal(filename);
	}
	else
	{
		std::string fullpath = searchFullPathForFilename(filename);
		if (fullpath.empty())
			return false;
		else
			return true;
	}
}

bool XUtilsFile::renameFile(const std::string &oldname, const std::string &name, const std::string &path)
{
	std::string oldPath = path + oldname;
	std::string newPath = path + name;

	// Rename a file
	int errorCode = MoveFile( (LPCWSTR)(oldPath.c_str()),  (LPCWSTR)(newPath.c_str()));

	if (0 != errorCode)
	{
		XLOGP("Fail to rename file %s to %s !Error code is %d", oldPath.c_str(), newPath.c_str(), errorCode);
		return false;
	}
	return true;
}

bool XUtilsFile::deleteFile(const std::string &path)
{
	std::string absolutePath = path;
	bool needCheckRelativeCache = false;
	if (!isAbsolutePath(path))
	{
		needCheckRelativeCache = true;
		absolutePath = searchFullPathForFilename(path);
	}
	if (!absolutePath.empty())
	{
		if (DeleteFile(LPCWSTR(absolutePath.c_str()))) {
			return false;
		}
		else {

			if (needCheckRelativeCache)
			{
				_fullPathCache.erase(path);
			}
			return true;
		}
	}
	return false;
}

std::string XUtilsFile::getFileData(const std::string& filename, const char* mode)
{
    XASSERT(!filename.empty() && mode != NULL, "Invalid parameters.");
	std::string data = "";
	std::ifstream ifs(filename.c_str());
	if (ifs)
	{
		std::stringstream buffer;
		buffer << ifs.rdbuf();
		data.assign(buffer.str());
	}
	else
	{
		XLOGP("%s file can not open",filename.c_str());
	}
	return data;
}

bool XUtilsFile::writeFileData(const std::string& fileName,const std::string& data)
{
	std::ofstream of(fileName.c_str());
	if (of.good())
	{
		of << data;
		of.close();
		return true;
	}
	else
	{
		XASSERT(0,fileName+" error in XUtilsFile::writeFileData");
	}
	return false;
}

std::vector<std::string> XUtilsFile::getFilesInDir(const std::string dir)
{
	std::vector<std::string> allFilePath;

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(LPCWSTR(dir.c_str()), &findFileData);
	std::string rootDir = "";
	rootDir.assign(dir);
	if (rootDir.find_last_of("*") != std::string::npos)
	{
		rootDir = rootDir.substr(0, rootDir.size() - 1);
	}
	if (rootDir.find_last_of("\\") != std::string::npos)
	{
		rootDir = rootDir.substr(0, rootDir.size() - 1);
	}
	std::string str = "";
	if (hFind != INVALID_HANDLE_VALUE)
	{
		while (::FindNextFile(hFind, &findFileData))
		{
			str = XUtilsFormatter::WC2UT(findFileData.cFileName);
			if (FILE_ATTRIBUTE_DIRECTORY & findFileData.dwFileAttributes)
			{
				if (str.compare("..") == 0 || str.compare(".") == 0)
				{
					continue;
				}
				else
				{

					std::vector<std::string> bunchV = XUtilsFile::getFilesInDir(rootDir + "\\" + str + "\\*");
					allFilePath.insert(allFilePath.end(), bunchV.begin(), bunchV.end());
				}
			}
			else
			{
				allFilePath.push_back(rootDir + "\\" + str);
			}
			
			
		}
		::FindClose(hFind);
	}
	return allFilePath;
}

std::string XUtilsFile::formatPath(const std::string& path)
{
	std::string returnPath = convertPathFormatToUnixStyle(path);
#ifdef WIN32
	
	if (path.find_first_of(":") != 1 && path.find_first_of(".\/") != 0 && path.find_first_of("file:") != 0)
	{
		returnPath.insert(0, ".\/");
	}

#endif
	return returnPath;
}

void XUtilsFile::checkDirAndCreate(std::string path)
{
	std::string formatPath = convertPathFormatToUnixStyle(path);
	char *fileName = const_cast<char*>(formatPath.c_str()), *tag;
	HANDLE hFile;
	hFile = CreateFile (TEXT("\\ONE.TXT"),      // Open One.txt
		GENERIC_READ,           // Open for reading
		0,                      // Do not share
		NULL,                   // No security
		OPEN_EXISTING,          // Existing file only
		FILE_ATTRIBUTE_NORMAL,  // Normal file
		NULL);                  // No template file
	for (tag = fileName; *tag; tag++)
	{
		if (*tag == '\/')
		{
			char buf[1000], path[1000];
			strcpy(buf, fileName);
			buf[strlen(fileName) - strlen(tag) + 1] = NULL;
			strcpy(path, buf);
			//if (ACCESS(path, 6) == -1)
			//{
			//	_mkdir(path);
			//}
		}
	}
}

XUtilsFile::XUtilsFile()
{
}

XUtilsFile::~XUtilsFile()
{
}
