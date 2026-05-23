#include "server_multi_io.hpp"
#include "../persing/persing_conf.hpp"
#include <iostream>
#include <exception>

int main(int ac, char **av)
{
    if (ac < 2)
    {
        std::cerr << "Usage: " << av[0] << " <config_file>" << std::endl;
        return 1;
    }

    Conf config;
    try
    {
        parsing_args(ac, av, config);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Config error: " << e.what() << std::endl;
        return 1;
    }

    try
    {
        Server serv;
        serv.boot_server(config);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
