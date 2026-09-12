#ifndef _WIN32
#include "http.hpp"
#include <curl/curl.h>
namespace { size_t sink(char* p,size_t s,size_t n,void* u){auto* out=static_cast<std::string*>(u);out->append(p,s*n);return s*n;} }
HttpResponse http_get(const std::string& url,int timeout_seconds){
    HttpResponse r;CURL* c=curl_easy_init();if(!c){r.error="curl init failed";return r;}curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,timeout_seconds);curl_easy_setopt(c,CURLOPT_TIMEOUT,timeout_seconds);curl_easy_setopt(c,CURLOPT_USERAGENT,"AdventureLandReferenceOS-Native/1.0");curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,sink);curl_easy_setopt(c,CURLOPT_WRITEDATA,&r.body);CURLcode rc=curl_easy_perform(c);long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);r.status=(int)code;if(rc!=CURLE_OK)r.error=curl_easy_strerror(rc);curl_easy_cleanup(c);return r;
}
#endif
