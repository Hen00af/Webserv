# HTTP/1.1 behavior

## Implemented

- HTTP/1.1 request line and required `Host` validation
- GET, HEAD, POST, and DELETE dispatch
- persistent connections by default, with `Connection: close` opt-out
- sequential and pipelined requests on one client connection
- `Content-Length` request framing
- `Transfer-Encoding: chunked`, chunk extensions, and trailers
- rejection of ambiguous `Content-Length` plus `Transfer-Encoding`
- configured request-body and header-overhead limits
- percent decoding, control-character rejection, and path normalization
- non-blocking client and CGI I/O in one `poll()` loop

HEAD uses the GET route and representation metadata. The server reports the
same `Content-Length` as GET but does not write the response body.

## Deliberate boundaries

- HTTP/2 and HTTP/3 are terminated by the front proxy.
- TLS is terminated by the front proxy.
- request transfer codings other than `chunked` are rejected.
- WebSocket upgrades are outside this server's scope.
- the bundled Control Plane is local-only and blocked by the production Caddy
  configuration.

The protocol integration test opens raw TCP sockets and verifies HEAD,
keep-alive pipelining, chunked CGI input, connection closure, and ambiguous
request-framing rejection.
