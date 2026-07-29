#!/usr/bin/env python3

import socket
import subprocess
import time


HOST = "127.0.0.1"
PORT = 8080


def connect():
    return socket.create_connection((HOST, PORT), timeout=3)


def read_response(stream, expect_body=True):
    status_line = stream.readline().decode("ascii").rstrip("\r\n")
    if not status_line:
        raise AssertionError("connection closed before response")
    headers = {}
    while True:
        line = stream.readline()
        if line == b"\r\n":
            break
        key, value = line.decode("iso-8859-1").split(":", 1)
        headers[key.lower()] = value.strip()
    length = int(headers.get("content-length", "0"))
    body = stream.read(length) if expect_body else b""
    return int(status_line.split(" ", 2)[1]), headers, body


def wait_until_ready():
    for _ in range(50):
        try:
            with connect() as client:
                client.sendall(
                    b"GET /health/ HTTP/1.1\r\n"
                    b"Host: localhost\r\nConnection: close\r\n\r\n"
                )
                return
        except OSError:
            time.sleep(0.1)
    raise AssertionError("webserv did not become ready")


def test_head():
    with connect() as client:
        client.sendall(
            b"HEAD / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
        )
        status, headers, body = read_response(client.makefile("rb"), False)
        assert status == 200
        assert int(headers["content-length"]) > 0
        assert headers["connection"] == "close"
        assert body == b""


def test_keep_alive_pipeline():
    with connect() as client:
        client.sendall(
            b"GET /health/ HTTP/1.1\r\nHost: localhost\r\n\r\n"
            b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
        )
        stream = client.makefile("rb")
        first = read_response(stream)
        second = read_response(stream)
        assert first[0] == 200
        assert first[1]["connection"] == "keep-alive"
        assert second[0] == 200
        assert second[1]["connection"] == "close"
        assert b"webserv is running" in second[2]


def test_chunked_cgi():
    with connect() as client:
        client.sendall(
            b"POST /cgi-bin/echo.py HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Transfer-Encoding: chunked\r\n"
            b"Content-Type: text/plain\r\n"
            b"Connection: close\r\n\r\n"
            b"5\r\nhello\r\n0\r\n\r\n"
        )
        status, _, body = read_response(client.makefile("rb"))
        assert status == 200
        assert b"body=hello" in body


def test_ambiguous_framing():
    with connect() as client:
        client.sendall(
            b"POST /upload HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Content-Length: 5\r\n"
            b"Transfer-Encoding: chunked\r\n"
            b"Connection: close\r\n\r\n"
            b"0\r\n\r\n"
        )
        status, _, _ = read_response(client.makefile("rb"))
        assert status == 400


def main():
    server = subprocess.Popen(
        ["./webserv", "config/default.conf"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        wait_until_ready()
        test_head()
        test_keep_alive_pipeline()
        test_chunked_cgi()
        test_ambiguous_framing()
    finally:
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
    print("HTTP protocol integration tests passed")


if __name__ == "__main__":
    main()
