#!/bin/sh
set -e

cd "$(dirname "$0")"

# NOTE: mock/conf.hpp が ../../persing/persing_conf.hpp を参照しており
# 現状では parseConf/ への移行未完。動作確認はその修正後に行うこと。
c++ -Wall -Wextra -std=c++98 \
    serializer_mock.cpp \
    ../serializer.cpp \
    ../builder.cpp \
    ../builder_util.cpp \
    ../../../mock/conf.cpp \
    ../../parseConf/parseConf.cpp \
    ../../parseConf/parseConfUtil.cpp \
    -o serializer_mock

echo "built: ./serializer_mock"
