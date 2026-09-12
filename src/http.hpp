#pragma once
#include <string>
struct HttpResponse { int status=0; std::string body; std::string error; bool ok() const { return status>=200&&status<300&&error.empty(); } };
HttpResponse http_get(const std::string& url,int timeout_seconds=25);
