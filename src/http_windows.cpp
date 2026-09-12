#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include "http.hpp"
#include <vector>
#pragma comment(lib,"winhttp.lib")
namespace { std::wstring widen(const std::string& s){if(s.empty())return{};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),w.data(),n);return w;} }
HttpResponse http_get(const std::string& url,int timeout_seconds){
    HttpResponse out;auto wurl=widen(url);URL_COMPONENTS uc{};uc.dwStructSize=sizeof(uc);uc.dwSchemeLength=(DWORD)-1;uc.dwHostNameLength=(DWORD)-1;uc.dwUrlPathLength=(DWORD)-1;uc.dwExtraInfoLength=(DWORD)-1;if(!WinHttpCrackUrl(wurl.c_str(),0,0,&uc)){out.error="WinHttpCrackUrl failed";return out;}std::wstring host(uc.lpszHostName,uc.dwHostNameLength),path(uc.lpszUrlPath,uc.dwUrlPathLength);if(uc.dwExtraInfoLength)path.append(uc.lpszExtraInfo,uc.dwExtraInfoLength);HINTERNET ses=WinHttpOpen(L"AdventureLandReferenceOS-Native/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!ses){out.error="WinHttpOpen failed";return out;}WinHttpSetTimeouts(ses,timeout_seconds*1000,timeout_seconds*1000,timeout_seconds*1000,timeout_seconds*1000);HINTERNET con=WinHttpConnect(ses,host.c_str(),uc.nPort,0);if(!con){WinHttpCloseHandle(ses);out.error="WinHttpConnect failed";return out;}DWORD flags=uc.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0;HINTERNET req=WinHttpOpenRequest(con,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,flags);if(!req){WinHttpCloseHandle(con);WinHttpCloseHandle(ses);out.error="WinHttpOpenRequest failed";return out;}BOOL ok=WinHttpSendRequest(req,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(req,nullptr);if(!ok){out.error="WinHTTP request failed: "+std::to_string(GetLastError());}else{DWORD code=0,len=sizeof(code);WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&len,WINHTTP_NO_HEADER_INDEX);out.status=(int)code;for(;;){DWORD avail=0;if(!WinHttpQueryDataAvailable(req,&avail)||!avail)break;std::vector<char>b(avail);DWORD got=0;if(!WinHttpReadData(req,b.data(),avail,&got))break;out.body.append(b.data(),got);}}WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return out;
}
#endif
