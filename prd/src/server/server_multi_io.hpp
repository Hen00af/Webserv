#ifndef SERVER_MULTI_IO_HPP
#define SERVER_MULTI_IO_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include <map>
#include "../persing/persing_conf.hpp"

struct ServerConfig;

class Conf;

/*
    このクラスはconfigからとってきた情報を元に、
    tcp/ip接続を行うクラス。

    HTTPのパースコンポーネント・バッファは持たない。
*/

class Server {
public:
    Server();
    ~Server();
    void    boot_server(Conf &conf);
    void    build_connection(const std::vector<ServerConfig> &servers);

private:
    std::vector<int> _server_fds;
};

#endif
