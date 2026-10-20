from __future__ import annotations

import re
import urllib.robotparser
from html.parser import HTMLParser
from urllib.parse import urljoin, urlsplit

from .core import DataError, utcnow
from .net import USER_AGENT


class ForumParser(HTMLParser):
    """Extract individual XenForo posts; keep quotations separate from original text."""
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.stack, self.posts, self.links = [], [], []
        self.current = None
        self.body_depth = None
        self.quote_depth = None
        self.next_page = None

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if tag not in {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr"}:
            self.handle_endtag(tag)

    def handle_starttag(self, tag, attrs):
        attr = dict(attrs)
        void = tag in {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr"}
        if not void:
            self.stack.append(tag)
        classes = attr.get("class", "").split()
        if tag == "article" and "message" in classes:
            self.current = {"post_id": attr.get("data-content", attr.get("id")), "author": attr.get("data-author"), "posted_at": None, "text_parts": [], "quotes": [], "links": []}
            self.posts.append(self.current)
        if tag == "time" and self.current and not self.body_depth and self.current["posted_at"] is None:
            self.current["posted_at"] = attr.get("datetime", self.current["posted_at"])
        if "bbWrapper" in classes and self.current:
            self.body_depth = len(self.stack)
        if tag == "blockquote" and self.body_depth and self.quote_depth is None:
            self.quote_depth = len(self.stack)
        if tag == "a" and attr.get("href"):
            href = attr["href"]
            self.links.append(href)
            if "next" in attr.get("rel", "").split() or "pageNav-jump--next" in classes:
                self.next_page = href
            if self.current and self.body_depth and not self.quote_depth:
                self.current["links"].append(href)
        if tag in {"br", "p", "div", "li"} and self.current and self.body_depth:
            self.current["quotes" if self.quote_depth else "text_parts"].append("\n")

    def handle_endtag(self, tag):
        if tag in {"br", "hr", "img", "input", "meta", "link"}:
            return
        if self.quote_depth == len(self.stack) and tag == "blockquote":
            self.quote_depth = None
        if self.body_depth == len(self.stack):
            self.body_depth = None
        if tag == "article":
            self.current = None
        if tag in self.stack:
            del self.stack[len(self.stack) - 1 - self.stack[::-1].index(tag):]

    def handle_data(self, text):
        if self.current and self.body_depth and "script" not in self.stack and "style" not in self.stack:
            target = "quotes" if self.quote_depth else "text_parts"
            self.current[target].append(text)

    def result(self):
        posts = []
        for post in self.posts:
            p = dict(post)
            p["text"] = re.sub(r"[ \t]+", " ", "".join(p.pop("text_parts"))).strip()
            p["quoted_text"] = re.sub(r"[ \t]+", " ", "".join(p.pop("quotes"))).strip()
            for text_key, field_key in (("text", "fields"), ("quoted_text", "quoted_fields")):
                p[field_key] = {}
                for key, value in re.findall(r"(?im)^\s*(?:[-*]\s*)?(Name|Term|Cycle Amnt|Cycle Amount|Total Amnt|Total Amount|Author|Receiver|Address|Created|Status|Vote Hash)\s*:\s*(.+)$", p[text_key]):
                    p[field_key][key.lower()] = value.strip()
            if p["text"]:
                posts.append(p)
        return posts


class ForumCollector:
    def __init__(self, client):
        self.client = client
        self.robot = None

    def allowed(self, url):
        if urlsplit(url).netloc != "forum.pivx.org" or not urlsplit(url).path.startswith(("/threads/", "/forums/")):
            raise DataError("Only public PIVX forum thread/index URLs are supported")
        if self.robot is None:
            body, _ = self.client.get("https://forum.pivx.org/robots.txt")
            self.robot = urllib.robotparser.RobotFileParser()
            self.robot.parse(body.decode().splitlines())
        if not self.robot.can_fetch(USER_AGENT, url):
            raise DataError("robots.txt disallows this URL")
        delay = self.robot.crawl_delay(USER_AGENT) or self.robot.crawl_delay("*")
        if delay:
            self.client.sleep(delay)

    def thread(self, url, max_pages=10):
        pages, posts, seen = [], [], set()
        current = url
        while current and current not in seen and len(pages) < max_pages:
            self.allowed(current)
            seen.add(current)
            body, meta = self.client.get(current)
            parser = ForumParser()
            parser.feed(body.decode("utf-8", errors="replace"))
            found = parser.result()
            if not found:
                raise DataError("No public posts extracted; possible login/challenge/markup change")
            pages.append(meta)
            posts.extend(found)
            current = urljoin(current, parser.next_page) if parser.next_page else None
        return {"url": url, "retrieved_at": utcnow(), "posts": posts, "pages": pages,
                "complete": current is None, "unfetched_next_page": current}

    def index(self, url, max_pages=3):
        current, seen, threads, pages = url, set(), set(), []
        while current and current not in seen and len(pages) < max_pages:
            self.allowed(current)
            seen.add(current)
            body, meta = self.client.get(current)
            parser = ForumParser()
            parser.feed(body.decode("utf-8", errors="replace"))
            pages.append(meta)
            for href in parser.links:
                link = urljoin(current, href)
                if urlsplit(link).netloc == "forum.pivx.org" and urlsplit(link).path.startswith("/threads/"):
                    path = urlsplit(link).path
                    canonical = re.match(r"(/threads/[^/]+/)", path + "/")
                    if canonical:
                        threads.add("https://forum.pivx.org" + canonical.group(1))
            current = urljoin(current, parser.next_page) if parser.next_page else None
        return {"url": url, "threads": sorted(threads), "pages": pages, "complete": current is None}


def get_price(client):
    url = "https://api.coingecko.com/api/v3/simple/price?ids=pivx&vs_currencies=usd&include_last_updated_at=true"
    data, source = client.json(url, fresh=True)
    try:
        pivx = data["pivx"]
        price, updated = pivx["usd"], pivx["last_updated_at"]
        from decimal import Decimal
        from datetime import datetime, timezone
        age = datetime.now(timezone.utc).timestamp() - updated
        if isinstance(price, bool) or not Decimal(str(price)).is_finite() or Decimal(str(price)) <= 0 or not -120 <= age <= 3600:
            raise ValueError("Price must be positive and updated within an hour")
        return {"usd_per_piv": str(price), "price_updated_at": datetime.fromtimestamp(updated, timezone.utc).isoformat(), "source": source}
    except (KeyError, TypeError, ValueError, ArithmeticError) as exc:
        raise DataError("Unavailable/stale USD conversion: %s" % exc)
