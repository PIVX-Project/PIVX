from __future__ import annotations

import hashlib
import ipaddress
import json
import socket
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import urlencode, urlsplit, urlunsplit

from .core import DataError, public_url, read_json, timestamp, utcnow, write_json

PROXIES = (
    "https://rpc.duddino.com/mainnet",
    "https://rpc.pivxla.bz/mainnet",
    "https://computingcache.com/pivx/rpc/mainnet",
)
USER_AGENT = "PIVX-Governance-Analyst/0.1 (public research; <=1 request/second)"
READ_METHODS = {"getblockcount", "getblockhash", "getblock", "getbudgetinfo", "getbudgetprojection", "getbudgetvotes"}


def ensure_public(url):
    public_url(url)
    host = urlsplit(url).hostname
    for result in socket.getaddrinfo(host, 443, type=socket.SOCK_STREAM):
        if not ipaddress.ip_address(result[4][0]).is_global:
            raise DataError("Refusing nonpublic network destination")


class Redirect(DataError):
    def __init__(self, url, status=301):
        super().__init__("Public redirect to " + url)
        self.url, self.status = url, status


class PublicRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        # Do not issue an unthrottled second HTTP request through urllib.
        raise Redirect(newurl, code)


def transport(url, timeout):
    ensure_public(url)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "application/json,text/html,text/plain"})
    with urllib.request.build_opener(PublicRedirect()).open(req, timeout=timeout) as response:
        body = response.read(5_000_001)
        if len(body) > 5_000_000:
            raise DataError("Response exceeds 5 MB limit")
        return response.status, dict(response.headers), body


class PublicClient:
    def __init__(self, root, *, timeout=8, retries=1, ttl=300, fetch=transport, clock=time.monotonic, sleep=time.sleep):
        self.root = Path(root)
        self.timeout, self.retries, self.ttl = timeout, retries, ttl
        self.fetch, self.clock, self.sleep = fetch, clock, sleep
        self.last_request = None
        self.events = []
        self.opener = None

    def get(self, url, *, fresh=False, _redirects=0):
        public_url(url)
        if _redirects > 5:
            raise DataError("Public redirect limit exceeded")
        key = hashlib.sha256(url.encode()).hexdigest()
        cache = self.root / "cache" / (key + ".json")
        if not fresh and cache.exists():
            record = read_json(cache)
            age = (datetime.now(timezone.utc) - timestamp(record["fetched_at"])).total_seconds()
            body_file = self.root / record["body_path"]
            if 0 <= age <= self.ttl and body_file.is_file():
                body = body_file.read_bytes()
                if hashlib.sha256(body).hexdigest() == record["sha256"]:
                    self.events.append({"url": url, "cache_hit": True, "fetched_at": record["fetched_at"]})
                    return body, record
        last_error = None
        for attempt in range(self.retries + 1):
            if self.last_request is not None:
                self.sleep(max(0, 1.01 - (self.clock() - self.last_request)))
            self.last_request = self.clock()
            fetched_at = utcnow()
            try:
                status, headers, body = self.fetch(url, self.timeout)
                if status != 200:
                    raise DataError("HTTP %s" % status)
                body_hash = hashlib.sha256(body).hexdigest()
                path = Path("raw") / (body_hash + ".body")
                destination = self.root / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(body)
                record = {"url": url, "fetched_at": fetched_at, "http_status": status,
                          "content_type": headers.get("Content-Type", headers.get("content-type", "")),
                          "sha256": body_hash, "body_path": path.as_posix()}
                self.events.append(record)
                write_json(self.root / "raw" / (key + "-" + fetched_at.replace(":", "") + ".json"), record)
                write_json(cache, record)
                return body, record
            except Redirect as exc:
                public_url(exc.url)
                self.events.append({"url": url, "fetched_at": fetched_at, "http_status": exc.status, "location": exc.url})
                return self.get(exc.url, fresh=fresh, _redirects=_redirects + 1)
            except (OSError, urllib.error.URLError, DataError) as exc:
                last_error = exc
                self.events.append({"url": url, "fetched_at": fetched_at, "attempt": attempt + 1, "error": str(exc)})
                if isinstance(exc, urllib.error.HTTPError) and exc.code not in (429, 500, 502, 503, 504):
                    break
                if attempt < self.retries:
                    self.sleep(min(4, 2 ** attempt))
        raise DataError("%s: %s" % (url, last_error))

    def json(self, url, *, fresh=False):
        body, record = self.get(url, fresh=fresh)
        try:
            return json.loads(body), record
        except (ValueError, UnicodeDecodeError) as exc:
            raise DataError("Malformed JSON at %s: %s" % (url, exc))

    def rpc(self, base, method, params=None, *, fresh=False):
        if base not in PROXIES or method not in READ_METHODS:
            raise DataError("Only named public proxies and read-only methods are allowed")
        url = base + "/" + method
        if params is not None:
            url += "?" + urlencode({"params": str(params)})
        return self.json(url, fresh=fresh)
