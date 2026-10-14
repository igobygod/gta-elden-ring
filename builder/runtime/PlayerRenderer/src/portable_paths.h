#pragma once
#include <windows.h>
#include <string>
#include <cstdio>
namespace igobygod {
// Asset and log paths are relative to this DLL, never to the game's cwd.
inline std::wstring wide(const char* utf8) {
 if(!utf8||!*utf8)return {};
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8,-1,nullptr,0);
 if(n<=0)return {};std::wstring out(size_t(n),L'\0');
 if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8,-1,out.data(),n))return {};
 out.resize(size_t(n-1));return out;
}
inline std::string utf8(const std::wstring& w) {
 int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);
 if(n<=0)return {};std::string out(size_t(n),'\0');
 WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),out.data(),n,nullptr,nullptr);return out;
}
inline const std::wstring& module_directory() {
 static const auto root=[] {
  HMODULE module=nullptr;
  if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&module_directory),&module))return std::wstring{};
  std::wstring path(32768,L'\0');DWORD n=GetModuleFileNameW(module,path.data(),DWORD(path.size()));
  if(!n||n>=path.size())return std::wstring{};path.resize(n);
  auto slash=path.find_last_of(L"\\/");return slash==std::wstring::npos?std::wstring{}:path.substr(0,slash);
 }();return root;
}
inline std::string data_path(const std::string& relative) {
 const auto& root=module_directory();if(root.empty())return {};
 return utf8(root+L"\\data\\"+wide(relative.c_str()));
}
inline std::string log_path(const std::string& relative) {
 const auto& root=module_directory();if(root.empty())return {};
 const auto dir=root+L"\\logs";CreateDirectoryW(dir.c_str(),nullptr);
 return utf8(dir+L"\\"+wide(relative.c_str()));
}
inline FILE* open(const char* path,const char* mode) {
 auto p=wide(path),m=wide(mode);if(p.empty()||m.empty())return nullptr;
 FILE* file=nullptr;return _wfopen_s(&file,p.c_str(),m.c_str())==0?file:nullptr;
}
}
