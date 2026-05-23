#include "test_handler.hpp"
#include <iostream>
#include <sstream>

// Mock buildResponse: 受け取ったHttpRequestの中身をdumpしつつ
// 検証可能なレスポンスを返す
HttpResponse buildResponse(const HttpRequest &req, const ResponseContext &ctx)
{
    std::cout << "=== Handler received HttpRequest ===" << std::endl;
    std::cout << "method:    " << req.method << std::endl;
    std::cout << "target:    " << req.target << std::endl;
    std::cout << "version:   " << req.version << std::endl;
    std::cout << "body size: " << req.body.size() << std::endl;
    if (!req.body.empty())
    {
        std::cout << "body content:" << std::endl;
        std::cout << "---" << std::endl;
        std::cout << req.body << std::endl;
        std::cout << "---" << std::endl;
    }
    std::cout << "headers (" << req.headers.size() << "):" << std::endl;

    std::map<std::string, std::string>::const_iterator it;
    for (it = req.headers.begin(); it != req.headers.end(); ++it)
    {
        std::cout << "  " << it->first << ": " << it->second << std::endl;
    }
    std::cout << "ctx.file_path: " << ctx.file_path << std::endl;
    std::cout << "=====================================" << std::endl;

    HttpResponse res;
    res.status_code = 200;
    res.reason_phrase = "OK";
    res.headers["Content-Type"] = "text/plain";
    res.body = "[mock] received " + req.method + " " + req.target + "\n";
    return res;
}

// Mock createResponse: HttpResponseを文字列に変換
std::string createResponse(const HttpResponse &res)
{
    std::ostringstream oss;

    oss << "HTTP/1.1 " << res.status_code << " " << res.reason_phrase << "\r\n";

    std::map<std::string, std::string>::const_iterator it;
    for (it = res.headers.begin(); it != res.headers.end(); ++it)
    {
        oss << it->first << ": " << it->second << "\r\n";
    }
    oss << "Content-Length: " << res.body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << res.body;

    return oss.str();
}
