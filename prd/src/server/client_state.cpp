#include "client_state.hpp"

// canonical
ClientState::ClientState()
    : _phase(READING_HEADERS),
      _header_end(0),
      _content_length(0),
      _last_activity(time(NULL))
{
    _read_buffer.reserve(BUF_MAX);
    _write_buffer.reserve(BUF_MAX);
}

ClientState::ClientState(const ClientState &other)
    : _read_buffer(other._read_buffer),
      _write_buffer(other._write_buffer),
      _phase(other._phase),
      _header_end(other._header_end),
      _content_length(other._content_length),
      _parser(other._parser),
      _body(other._body),
      _last_activity(other._last_activity) 
{
}

ClientState &ClientState::operator=(const ClientState &other)
{
    if (this != &other)
    {
        _read_buffer = other._read_buffer;
        _write_buffer = other._write_buffer;
        _phase = other._phase;
        _header_end = other._header_end;
        _content_length = other._content_length;
        _parser = other._parser;
        _body = other._body;
        _last_activity = other._last_activity;
    }
    return *this;
}

ClientState::~ClientState() {}

// 受信メソッド
void ClientState::appendRead(const char *data, size_t n)
{
    _read_buffer.append(data, n);
}

const std::string &ClientState::getReadBuffer() const
{
    return _read_buffer;
}

// 送信メソッド
void ClientState::setWriteBuffer(const std::string &res)
{
    _write_buffer = res;
}

std::string &ClientState::getWriteBuffer()
{
    return _write_buffer;
}

// フェーズ
Phase ClientState::getPhase() const
{
    return _phase;
}

void ClientState::setPhase(Phase p)
{
    _phase = p;
}

// ヘッダー終端
size_t ClientState::getHeaderEnd() const
{
    return _header_end;
}

void ClientState::setHeaderEnd(size_t pos)
{
    _header_end = pos;
}

// Content-Length
size_t ClientState::getContentLength() const
{
    return _content_length;
}

void ClientState::setContentLength(size_t len)
{
    _content_length = len;
}

void ClientState::clearReadBuffer()
{
    _read_buffer.clear();
}

RequestParser &ClientState::getParser()
{
    return _parser;
}

const std::string &ClientState::getBody() const 
{
    return _body;
}

void ClientState::appendBody(const char *data, size_t n)
{
    _body.append(data, n);
}

size_t ClientState::getBodyReceived() const
{
    return _body.size();
}

void ClientState::updateActivity()
{
    _last_activity = time(NULL);
}

time_t ClientState::getLastActivity() const
{
    return _last_activity;
}

bool ClientState::isTimedOut(time_t now, time_t timeout) const
{
    return (now - _last_activity) > timeout;
}