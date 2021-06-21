#include "StdAfx.h"
#include "FileSystem.h"
//#include <io.h>	// _A_SUBDIR, _findclose()
#ifdef __APPLE__
#include <sys/uio.h>
#include <dirent.h>
#else
#include <io.h>
#endif

// todo: move
//https://riptutorial.com/cplusplus/example/4190/conversion-to-std--wstring
#include <codecvt>
#include <locale>

using convert_t = std::codecvt_utf8<wchar_t>;
std::wstring_convert<convert_t, wchar_t> strconverter;

std::string to_string(std::wstring wstr) {
	return strconverter.to_bytes(wstr);
}

std::wstring to_wstring(std::string str) {
	return strconverter.from_bytes(str);
}


FilePath::FilePath(const char* in) {
	assert(in);

	if (in)
		path = in;
}

static bool IsAnySlash(const char c) {
	return c == '\\' || c== '/';
}

const char* FilePath::GetExtension() const {
	assert(IsValid());

	const char* p = &path[0];
	const char* ret = nullptr;

	while (*p) {
		if (IsAnySlash(*p))
			ret = nullptr;
		else if (*p == extensionChar)
			ret = p + 1;

		++p;
	}

	if (!ret)
		ret = p;

	return ret;
}

bool FilePath::RemoveExtension() {
	if(path.empty())
		return false;

	const char* extension = GetExtension();

	// remove '.' as well
	if(extension > &path.front() && extension[-1] == extensionChar)
		--extension;

	size_t extensionLen = &path.back() + 1 - extension;

	if(extensionLen == 0)
		return false;
	
	path.resize(path.size() - extensionLen);
	return true;
}

void FilePath::Normalize() {
	assert(IsValid());

	char* p = &path[0];

	while(*p) {
		if(*p == '\\')
			*p = '/';
		++p;
	}
}

void FilePath::Append(const char* rhs) {
	assert(rhs);
	assert(IsValid());

	// to prevent crash with bad input
	if (!rhs)
		return;

	if(!path.empty())
		path += (char)'/';

	path += rhs;
}

bool FilePath::IsValid() const {
	if (path.empty())
		return true;
	
	return !IsAnySlash(path.back());
}
// ---------------------------------------------------------------------------

// EndsWith("Any","") => true
// case in sensitive
bool EndsWith(const char* name, const char* end) {
	size_t n = strlen(name);
	size_t e = strlen(end);
	if (n < e)
		return false;

	return _stricmp(name + n - e, end) == 0;
}

void DirectoryTraverse(IDirectoryTraverse& sink, const FilePath& filePath, const char* fileEnd) {
	assert(fileEnd);
	sink.OnStart();

	// don't use file as pattern as it will also filter directories
	FilePath pathWithPattern = filePath;
	pathWithPattern.Append("*.*");

#ifdef WIN32
	// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/findfirst-functions?view=vs-2019
	struct _finddata_t c_file;
	intptr_t hFile = 0;

	if ((hFile = _findfirst(GetAbsPath(pathWithPattern.path.c_str()).c_str(), &c_file)) == -1L)
		return;

	do
	{
		if (strcmp(c_file.name, ".") == 0 || strcmp(c_file.name, "..") == 0)
				continue;

		if (c_file.attrib & _A_SUBDIR) {
			FilePath pathWithDirectory = filePath;
			pathWithDirectory.Append(c_file.name);

			if (sink.OnDirectory(pathWithDirectory, c_file.name)) {
				// recursion
				DirectoryTraverse(sink, pathWithDirectory, fileEnd);
			}
		} else {
			if(EndsWith(c_file.name, fileEnd))
				sink.OnFile(filePath, c_file.name);
		}
	} while (_findnext(hFile, &c_file) == 0);

	_findclose(hFile);

#else
	DIR* dp;
	struct dirent* entry;

	//	std::string utf8 = to_string(pathWithPattern.path);

	//    if ((dp = opendir(utf8.c_str())) == NULL)
	if ((dp = opendir(pathWithPattern.path.c_str())) == NULL)
		return;

	while ((entry = readdir(dp))) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		//		std::wstring name = to_wstring(entry->d_name);

		if (entry->d_type == DT_DIR) {
			//if (c_file.attrib & _A_SUBDIR) {
			FilePath pathWithDirectory = filePath;

			pathWithDirectory.Append(entry->d_name);

			if (sink.OnDirectory(pathWithDirectory, entry->d_name)) {
				// recursion
				DirectoryTraverse(sink, pathWithDirectory, pattern);
			}
		}
		else {
			sink.OnFile(filePath, entry->d_name);
		}
	}

	closedir(dp);

#endif


	sink.OnEnd();
}

void FilePath::Test() {
	// GetExtension()

	{
		std::string a = FilePath("c:\\temp\\a.ext").GetExtension();
		assert(a == "ext");
	}
	{
		std::string a = FilePath("a.ext").GetExtension();
		assert(a == "ext");
	}
	{
		std::string a = FilePath("c:\\aa.ss\\a.ext").GetExtension();
		assert(a == "ext");
	}
	{
		std::string a = FilePath("c:\\aa.ss\\a").GetExtension();
		assert(a == "");
	}
	{
		std::string a = FilePath("file name a").GetExtension();
		assert(a == "");
	}
	{
		std::string a = FilePath("./aa.ss/a").GetExtension();
		assert(a == "");
	}
	{
		std::string a = FilePath("").GetExtension();
		assert(a == "");
	}
	{
		FilePath path;
		std::string a = path.GetExtension();
		assert(a == "");
	}

	// IsValid()

	{
		FilePath A("c:\\aa.ss\\a.ext");
		assert(A.IsValid());
	}
	{
		FilePath A("");
		assert(A.IsValid());
	}
	{
		FilePath A("/");
		assert(!A.IsValid());
	}
	{
		FilePath A("\\");
		assert(!A.IsValid());
	}
	{
		FilePath A("\\a");
		assert(A.IsValid());
	}
	{
		FilePath A("c:");
		assert(A.IsValid());
	}
	{
		FilePath A("c:\\");
		assert(!A.IsValid());
	}
	{
		FilePath A("c:/");
		assert(!A.IsValid());
	}

	// Append()

	{
		FilePath A("");
		A.Append("");
		assert(A.path == "");
	}
	{
		FilePath A("");
		A.Append("file");
		assert(A.path == "file");
	}
	{
		FilePath A("c:");
		A.Append("file");
		assert(A.path == "c:/file");
	}
	{
		FilePath A("path/dir");
		A.Append("file");
		assert(A.path == "path/dir/file");
	}
	{
		FilePath A("");
		A.Append("file");
		assert(A.path == "file");
	}


	// Normalize()

	{
		FilePath A("c:\\aa.ss\\a.ext");
		A.Normalize();
		assert(A.path == "c:/aa.ss/a.ext");
	}
	{
		FilePath A("D:/aa\\a.ext");
		A.Normalize();
		assert(A.path == "D:/aa/a.ext");
	}
	{
		FilePath A("");
		A.Normalize();
		assert(A.path == "");
	}
	{
		FilePath A;
		A.Normalize();
		assert(A.path == "");
	}

	// RemoveExtension()

	{
		FilePath A("c:/aa.ss\\a");
		A.RemoveExtension();
		assert(A.path == "c:/aa.ss\\a");
	}
	{
		FilePath A("D:/aa.ss\\a.ext");
		A.RemoveExtension();
		assert(A.path == "D:/aa.ss\\a");
	}
	{
		FilePath A("D:/aa\\b.");
		A.RemoveExtension();
		assert(A.path == "D:/aa\\b");
	}
	{
		FilePath A("");
		A.RemoveExtension();
		assert(A.path == "");
	}
	{
		FilePath A;
		A.RemoveExtension();
		assert(A.path == "");
	}
}

