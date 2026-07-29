# Production deployment boundary

Webserv intentionally serves plain HTTP. In production, TLS termination,
certificate renewal, public rate limiting, and access control belong in a
front proxy or managed load balancer.

The repository includes `compose.production.yaml` and `deploy/Caddyfile` as a
reference deployment. Caddy exposes ports 80 and 443, obtains and renews the
certificate for `WEBSERV_DOMAIN`, adds basic security headers, and proxies to
Webserv over the private Compose network.

The production proxy returns `404` for `/admin` and `/admin/*`. The Control
Plane is a local development instrument and has no authentication. Do not
publish it directly.

## Start

Point the domain's A/AAAA record at the host, allow inbound TCP 80/443 and UDP
443, then run:

```sh
WEBSERV_DOMAIN=webserv.example.com \
  docker compose -f compose.production.yaml up --build -d
```

Verify:

```sh
curl -I https://webserv.example.com/health/
curl -I https://webserv.example.com/admin/
```

The health endpoint should return `200`; the production proxy should return
`404` for the Control Plane.

For a managed platform, use its TLS and rate-limiting features instead of the
bundled Caddy service. Keep Webserv's port 8080 private.
