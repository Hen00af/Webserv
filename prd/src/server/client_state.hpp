#ifndef CLIENT_STATE_HPP
#define CLIENT_STATE_HPP

#include <string>
#include <cstddef>
#include <ctime>

#include "../persing/persing_request.hpp"

#define BUF_MAX 4096

enum Phase
{
    READING_HEADERS,
    READING_BODY,
    WRITING_RESPONSE,
    DONE
};

class ClientState
{
private:
    std::string _read_buffer;
    std::string _write_buffer;
    Phase _phase;
    size_t _header_end;
    size_t _content_length;
    RequestParser _parser;
    std::string _body;
    time_t _last_activity;

public:
    // カノニカル
    ClientState();
    ClientState(const ClientState &other);
    ClientState &operator=(const ClientState &other);
    ~ClientState();

    // 受信メソッド
    void appendRead(const char *data, size_t n);
    const std::string &getReadBuffer() const;
    void clearReadBuffer();

    // 送信メソッド
    void setWriteBuffer(const std::string &res);
    std::string &getWriteBuffer();

    // フェーズ用メソッド
    Phase getPhase() const;
    void setPhase(Phase p);

    // ヘッダー終端位置
    size_t getHeaderEnd() const;
    void setHeaderEnd(size_t pos);

    // Content-Length
    size_t getContentLength() const;
    void setContentLength(size_t len);

    RequestParser &getParser();
    const std::string &getBody() const;
    void appendBody(const char *data, size_t n);
    size_t getBodyReceived() const;
    void updateActivity();
    time_t getLastActivity() const;
    bool isTimedOut(time_t now, time_t timeout) const;
};

#endif
