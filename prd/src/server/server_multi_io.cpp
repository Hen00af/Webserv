// #include <iostream>
// #include <cstring>
// #include <cerrno>
// #include <unistd.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <fcntl.h>
// #include <poll.h>
// #include <map>
#include <sstream>
// #include "../persing/persing_conf.hpp"
#include "server_multi_io.hpp"
#include "client_state.hpp"
#include "../http/request.hpp"
#ifdef TEST_BUILD
#include "../test/test_handler.hpp"
#else
#include "../responsebuilder/responsebuilder/responsebuilder.hpp"
#include "../responsebuilder/responseSerializer/responseSerializer.hpp"
#endif

#define MEM_LIMIT 4096
#define MAX_HEADER_SIZE 8192
#define MAX_BODY_SIZE 1048576 // 1MB (本来はconfigから)
#define TIMEOUT_SEC 5

static HttpRequest make_request(const RequestParser &parser, const std::string &body)
{
    HttpRequest req;
    req.method = parser.getMethod();
    req.target = parser.getTarget();
    req.version = parser.getVersion();
    req.headers = parser.getHeaders();
    req.body = body;
    return req;
}

static std::string make_error_response(int status, const std::string &msg)
{
    std::ostringstream body;
    body << "<html><body><h1>" << status << " " << msg << "</h1></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << msg << "\r\n"
        << "Content-Type: text/html\r\n"
        << "Content-Length: " << body.str().size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body.str();
    return oss.str();
}

class Conf;

void Server::build_connection(const std::vector<ServerConfig> &servers)
{
    if (servers.empty())
        throw Boot_serverErr();
    for (size_t i = 0; i < servers.size(); i++)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1)
        {
            std::cerr << "socket error: " << std::strerror(errno) << std::endl;
            throw Boot_serverErr();
        }
        const int enable = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                       &enable, sizeof(enable)) == -1)
        {
            std::cerr << "setsockopt error: " << std::strerror(errno) << std::endl;
            close(fd);
            throw Boot_serverErr();
        }
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;

        std::istringstream iss(servers[i].listen);
        int port;
        iss >> port;
        addr.sin_port = htons(port);
        if (bind(fd,
                 reinterpret_cast<struct sockaddr *>(&addr),
                 sizeof(addr)) == -1)
        {
            std::cerr << "bind error on port " << port << std::endl;
            close(fd);
            throw Boot_serverErr();
        }

        if (listen(fd, SOMAXCONN) == -1)
        {
            std::cerr << "listen error" << std::endl;
            close(fd);
            throw Boot_serverErr();
        }
        fcntl(fd, F_SETFL, O_NONBLOCK);
        _server_fds.push_back(fd);
        std::cout << "listening on port " << port << std::endl;
    }
}

void Server::boot_server(Conf &conf)
{
    const std::vector<ServerConfig> &servers = conf.get_servers();
    Server::build_connection(servers);
    std::vector<struct pollfd> fds;
    std::map<int, ClientState> clients;

    for (size_t i = 0; i < _server_fds.size(); i++)
    {
        struct pollfd p;
        p.fd = _server_fds[i];
        p.events = POLLIN;
        p.revents = 0;
        fds.push_back(p);
    }
    while (true)
    {
        int ready = poll(&fds[0], fds.size(), 1000);
        if (ready == -1)
        {
            std::cerr << "poll error" << std::endl;
            break;
        }
        time_t now = time(NULL);
        for (size_t i = 0; i < fds.size(); i++)
        {
            bool is_server = false;
            for (size_t j = 0; j < _server_fds.size(); j++)
            {
                if (fds[i].fd == _server_fds[j])
                {
                    is_server = true;
                    break;
                }
            }
            if (is_server)
                continue;
            if (clients[fds[i].fd].getPhase() == WRITING_RESPONSE)
                continue;
            if (clients[fds[i].fd].isTimedOut(now, TIMEOUT_SEC))
            {
                std::cerr << "timeout: fd=" << fds[i].fd << std::endl;
                ClientState &cs = clients[fds[i].fd];
                cs.setWriteBuffer(make_error_response(408, "Request Timeout"));
                fds[i].events = POLLOUT;
                cs.setPhase(WRITING_RESPONSE);
            }
        }
        for (size_t i = 0; i < fds.size(); i++)
        {
            if (fds[i].revents == 0)
                continue;
            // TODO サーバーFD処理
            bool is_server_fd = false;
            for (size_t k = 0; k < _server_fds.size(); k++)
            {
                if (fds[i].fd == _server_fds[k])
                {
                    is_server_fd = true;
                    break;
                }
            }
            if (is_server_fd)
            {
                int client_fd = accept(fds[i].fd, NULL, NULL); // ★ fds[i].fd ★
                if (client_fd == -1)
                {
                    std::cerr << "accept error" << std::endl;
                    continue;
                }
                fcntl(client_fd, F_SETFL, O_NONBLOCK);

                struct pollfd client_pfd;
                client_pfd.fd = client_fd;
                client_pfd.events = POLLIN;
                client_pfd.revents = 0;
                fds.push_back(client_pfd);

                clients[client_fd] = ClientState();
                std::cout << "new client: fd = " << client_fd << std::endl;
                continue;
            }
            // TODOクライアントFD　POLLIN処理
            if (fds[i].revents & POLLIN)
            {
                char buf[BUF_MAX];
                ssize_t n = recv(fds[i].fd, buf, sizeof(buf), 0);
                if (n <= 0)
                {
                    if (n == 0)
                        std::cout << "client disconnected: fd = " << fds[i].fd << std::endl;
                    else
                        std::cerr << "recv error" << std::endl;
                    close(fds[i].fd);
                    clients.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }
                ClientState &cs = clients[fds[i].fd];
                cs.appendRead(buf, n);
                cs.updateActivity();
                // Dos対策１：ヘッダー上限チェック
                if (cs.getPhase() == READING_HEADERS && cs.getReadBuffer().size() > MAX_HEADER_SIZE)
                {
                    std::cerr << "header too large" << std::endl;
                    cs.setWriteBuffer(make_error_response(431, "Request Header Fields Too Large"));
                    fds[i].events = POLLOUT;
                    cs.setPhase(WRITING_RESPONSE);
                    continue;
                }
                // Phase：ヘッダー受信中
                if (cs.getPhase() == READING_HEADERS)
                {
                    size_t end = cs.getReadBuffer().find("\r\n\r\n");
                    if (end == std::string::npos) // HEADER終わっていない
                        continue;
                    cs.setHeaderEnd(end + 4);

                    std::string headers_part = cs.getReadBuffer().substr(0, cs.getHeaderEnd());
                    try
                    {
                        if (!cs.getParser().parseRequest(headers_part))
                        {
                            std::cerr << "parse error" << std::endl;
                            cs.setWriteBuffer(make_error_response(400, "Bad Request"));
                            fds[i].events = POLLOUT;
                            cs.clearReadBuffer();
                            cs.setPhase(WRITING_RESPONSE);
                            continue;
                        }
                    }
                    catch (const VersionNotSupported &)
                    {
                        std::cerr << "version not supported" << std::endl;
                        cs.setWriteBuffer(make_error_response(505, "HTTP Version Not Supported"));
                        fds[i].events = POLLOUT;
                        cs.clearReadBuffer();
                        cs.setPhase(WRITING_RESPONSE);
                        continue;
                    }
                    catch (const std::exception &)
                    {
                        std::cerr << "parser exception" << std::endl;
                        cs.setWriteBuffer(make_error_response(400, "Bad Request"));
                        fds[i].events = POLLOUT;
                        cs.clearReadBuffer();
                        cs.setPhase(WRITING_RESPONSE);
                        continue;
                    }
                    // Content-length取得
                    cs.setContentLength(cs.getParser().getContentLength());
                    // Dos対策２：Content-length上限チェック
                    if (cs.getContentLength() > MAX_BODY_SIZE)
                    {
                        std::cerr << "body too large (declared)" << std::endl;
                        cs.setWriteBuffer(make_error_response(413, "Payload Too Large"));
                        fds[i].events = POLLOUT;
                        cs.clearReadBuffer();
                        cs.setPhase(WRITING_RESPONSE);
                        continue;
                    }
                    // すでに受信済みのボディ部分を_bodyにコピー
                    if (cs.getReadBuffer().size() > cs.getHeaderEnd())
                    {
                        std::string body_part = cs.getReadBuffer().substr(cs.getHeaderEnd());
                        cs.appendBody(body_part.c_str(), body_part.size());
                    }
                    cs.clearReadBuffer();
                    cs.setPhase(READING_BODY);
                }

                // Phase：ボディ受信中
                if (cs.getPhase() == READING_BODY)
                {
                    if (!cs.getReadBuffer().empty())
                    {
                        const std::string &rb = cs.getReadBuffer();
                        cs.appendBody(rb.c_str(), rb.size());
                        cs.clearReadBuffer();
                    }
                    // Dos対策３：ボディ上限チェック
                    if (cs.getBodyReceived() > MAX_BODY_SIZE)
                    {
                        std::cerr << "body too large (received)" << std::endl;
                        cs.setWriteBuffer(make_error_response(413, "Payload Too Large"));
                        fds[i].events = POLLOUT;
                        cs.setPhase(WRITING_RESPONSE);
                        continue;
                    }
                    if (cs.getBodyReceived() < cs.getContentLength())
                        continue;
                    // TODO: Handler呼び出し→レスポンス生成（仮）
                    HttpRequest req = make_request(cs.getParser(), cs.getBody());
                    std::cout << "Request: " << req.method << " " << req.target << std::endl;
                    ResponseContext ctx;
                    ctx.config = &conf;
                    if (req.target == "/")
                        ctx.file_path = "./www/index.html";
                    else
                        ctx.file_path = "./www" + req.target;
                    HttpResponse res = buildResponse(req, ctx);
                    std::string response = createResponse(res);
                    cs.setWriteBuffer(response);
                    fds[i].events = POLLOUT;
                    cs.setPhase(WRITING_RESPONSE);
                }
            }
            // TODOクライアントFD　POLLOUT処理
            if (fds[i].revents & POLLOUT)
            {
                ClientState &cs = clients[fds[i].fd];
                std::string &w_buf = cs.getWriteBuffer();

                ssize_t sent = send(fds[i].fd, w_buf.c_str(), w_buf.size(), 0);
                if (sent == -1)
                {
                    std::cerr << "send error" << std::endl;
                    close(fds[i].fd);
                    clients.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }
                cs.updateActivity();
                // 送れた分をバッファから削除
                w_buf.erase(0, sent);
                // 全部送り終えたら切断
                if (w_buf.empty())
                {
                    cs.setPhase(DONE);
                    close(fds[i].fd);
                    clients.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                }
            }
        }
    }
}

Server::Server() {}

Server::~Server()
{
    for (size_t i = 0; i < _server_fds.size(); i++)
    {
        if (_server_fds[i] != -1)
        {
            close(_server_fds[i]);
        }
    }
}

void tcp(Conf &conf)
{
    Server serv;
    serv.boot_server(conf);
}
