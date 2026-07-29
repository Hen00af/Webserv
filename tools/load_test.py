#!/usr/bin/env python3

import argparse
import concurrent.futures
import http.client
import statistics
import time


HOST = "127.0.0.1"
PORT = 8080
MAX_REQUESTS = 1000
MAX_CONCURRENCY = 32
TARGETS = {
    "health": "/health/",
    "home": "/",
    "python-cgi": "/cgi-bin/hello.py",
    "php-cgi": "/cgi-bin/hello.php",
}


def percentile(values, percentage):
    ordered = sorted(values)
    index = max(0, (len(ordered) * percentage + 99) // 100 - 1)
    return ordered[min(index, len(ordered) - 1)]


def request_once(path):
    started = time.perf_counter()
    status = 0
    try:
        connection = http.client.HTTPConnection(HOST, PORT, timeout=10)
        connection.request(
            "GET",
            path,
            headers={"Host": "localhost", "Connection": "close"},
        )
        response = connection.getresponse()
        status = response.status
        response.read()
        connection.close()
    except OSError:
        pass
    return status, (time.perf_counter() - started) * 1000


def main():
    parser = argparse.ArgumentParser(description="Safe localhost HTTP/1.1 load test")
    parser.add_argument("target", choices=sorted(TARGETS))
    parser.add_argument("-n", "--requests", type=int, default=100)
    parser.add_argument("-c", "--concurrency", type=int, default=8)
    args = parser.parse_args()

    total = min(max(args.requests, 1), MAX_REQUESTS)
    concurrency = min(max(args.concurrency, 1), MAX_CONCURRENCY)
    started = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=concurrency
    ) as executor:
        results = list(
            executor.map(lambda _: request_once(TARGETS[args.target]), range(total))
        )
    elapsed = time.perf_counter() - started
    latencies = [latency for _, latency in results]
    successes = sum(200 <= status < 400 for status, _ in results)
    statuses = {}
    for status, _ in results:
        statuses[status] = statuses.get(status, 0) + 1

    print("target={}".format(args.target))
    print("requests={} concurrency={}".format(total, concurrency))
    print("success={} errors={}".format(successes, total - successes))
    print("statuses={}".format(statuses))
    print("throughput={:.2f} req/s".format(total / elapsed))
    print("mean={:.2f} ms".format(statistics.mean(latencies)))
    print("median={:.2f} ms".format(statistics.median(latencies)))
    print("p95={:.2f} ms".format(percentile(latencies, 95)))
    print("max={:.2f} ms".format(max(latencies)))

    if successes != total:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
