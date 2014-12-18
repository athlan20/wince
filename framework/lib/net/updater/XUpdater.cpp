#include "XUpdater.h"
#include "../../../config.h"
#include "../../base/macros.h"
#include "../XDownloader.h"
#include "../../utils/XUtilsFile.h"


#include "json/json.h"
#include <iostream>
#include <cstdio>

#if (CC_TARGET_PLATFORM != CC_PLATFORM_WIN32)
#include <dirent.h>
#include <sys/stat.h>
#endif

#define DOWNLOAD_FIEL	"download"	//下载后保存的文件夹名

XUpdater::XUpdater(const char* packageUrl/* =nullptr */, const char* resVersionUrl/* =nullptr */, const char* storagePath/* =nullptr */) :
_packageUrl(packageUrl),
_resVersionUrl(resVersionUrl),
_storagePath(storagePath)
{

}

XUpdater::~XUpdater()
{

}

bool XUpdater::init()
{
	initDownloadDir();

	return true;
}

bool XUpdater::checkVersion(std::string localVersionPath)
{
	std::string serverVer = this->getMainVersion();
	std::string localVer = this->getLocalMainVersion(localVersionPath);
	bool isSame = false;
	XASSERT(serverVer!="","serverVer get empty");
	if (serverVer!="" || localVer!="")
	{
		if (localVer.compare(serverVer) == 0)
		{
			isSame = true;
		}
	}
	return isSame;
}

std::string XUpdater::getLocalMainVersion(std::string mainVersionUrl)		//获取本地目录
{
	mainVersionUrl = mainVersionUrl == "" ? "resource.json" : mainVersionUrl;
	size_t size = 0;
	std::string versionStr = "";
	std::string versionData = XUtilsFile::getFileData(mainVersionUrl);
	if (versionData!="")
	{
		Json::Value root;
		Json::Reader jReader;
		jReader.parse(versionData, root);
		versionStr = root["version"].asString();
	}
	
	return versionStr;
}

void XUpdater::initDownloadDir()
{

}

std::string XUpdater::getMainVersion(std::string mainVersionUrl)
{
	mainVersionUrl = mainVersionUrl == "" ? this->_resVersionUrl : mainVersionUrl;
	std::shared_ptr<XDownloader> downloader = std::shared_ptr<XDownloader>(new XDownloader);
	downloader->downloadSync(mainVersionUrl.c_str(),this->_storagePath+"tempMainVersion");
	//json 文档
	Json::Value root;
	Json::Reader jReader;
	std::string data = XUtilsFile::getFileData(this->_storagePath + "tempMainVersion");
	XUtilsFile::renameFile("tempMainVersion", "resource.json", this->_storagePath);
	if (data != "")
	{
		jReader.parse(data, root);
		this->_serverResJson = root;
		std::string version = root["version"].asString();
		return version;
	}
	return "";
}

/************************************************************************/
/* 进度回调                                                             */
/************************************************************************/
void XUpdater::onError()
{

}
void XUpdater::onProgress(int percent)
{

}
void XUpdater::onSuccess()
{
}

void XUpdater::reset()
{

}

void XUpdater::upgrade(std::string resourcePath)
{
	bool isSame = this->checkVersion(resourcePath);
	if (!isSame)
	{
		Json::Reader jReader;
		Json::Value root;
		std::string data = XUtilsFile::getFileData(resourcePath);
		jReader.parse(data, root);
		Json::Value files = root["files"];
		
		Json::Value serverJson = this->_serverResJson["files"];
		Json::Value::Members member = serverJson.getMemberNames();
		std::unordered_map<std::string, XDownloader::XDownloadUnit> units;
		for (Json::Value::Members::iterator iter = member.begin(); iter != member.end(); iter++)
		{
			std::string serverFileKey = *iter;
			std::string serverFileMD5 = serverJson[serverFileKey].asString();
			if (files[serverFileKey].isNull() 
				|| files[serverFileKey].asString().compare(serverFileMD5) != 0 
				|| !XUtilsFile::isFileExist(serverFileKey))	//不存在或者不一样
			{
				XDownloader::XDownloadUnit unit;
				unit.srcUrl = this->_packageUrl + serverFileKey;
				unit.storagePath = this->_storagePath + serverFileKey;
				units[serverFileKey] = unit;
			}
		}
		if (units.size() > 0)
		{
			this->_downloader = std::shared_ptr<XDownloader>(new XDownloader());
			this->_downloader->queueDownloadSync(units);
		}
	}
}