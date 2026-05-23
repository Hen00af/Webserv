#ifndef TEST_HANDLER_HPP
#define TEST_HANDLER_HPP

#include <string>
#include <map>
#include "../http/request.hpp"
#include "../persing/persing_conf.hpp"

// 本物 (responsebuilder) と同じ構造体定義
// TEST_BUILD 時はこちらが使われる

struct HttpResponse
{
    int                                 status_code;
    std::string                         reason_phrase;
    std::map<std::string, std::string>  headers;
    std::string                         body;
};

struct ResponseContext
{
    const Conf  *config;
    std::string file_path;
};

// 本物と同じシグネチャの Mock 関数
HttpResponse buildResponse(const HttpRequest &req, const ResponseContext &ctx);
std::string  createResponse(const HttpResponse &res);

#endif
