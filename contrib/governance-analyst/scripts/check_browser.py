"""Optional real-browser and independent schema checks; requires requirements-dev.txt and Chrome."""
import json
import sys
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import unquote, urlsplit
from jsonschema import Draft202012Validator, FormatChecker
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
report = json.loads((ROOT / 'data/drafts/initial-report.json').read_text())
schema = json.loads((ROOT / 'governance.schema.json').read_text())
Draft202012Validator.check_schema(schema)
Draft202012Validator(schema, format_checker=FormatChecker()).validate(report)
base = sys.argv[1] if len(sys.argv) > 1 else 'http://127.0.0.1:8768/'
results, errors = [], []
(ROOT / 'data/verification').mkdir(parents=True, exist_ok=True)
pages = ['index.html', 'history.html', 'methodology.html', 'archive/index.html'] + ['proposals/' + p['id'] + '.html' for p in report['proposals']]
with sync_playwright() as p:
    browser = p.chromium.launch(channel='chrome', headless=True)
    for width in (1440, 390):
        page = browser.new_page(viewport={'width': width, 'height': 1000}, device_scale_factor=1, reduced_motion='reduce')
        page.on('pageerror', lambda e: errors.append(str(e)))
        page.goto(base + 'index.html', wait_until='networkidle')
        assert page.locator('[data-proposal]:visible').count() == 6
        for text, count in [(report['proposals'][0]['proposer'], 2), ('no-such-proposal', 0), ('', 6)]:
            page.locator('#proposal-search').fill(text)
            assert page.locator('[data-proposal]:visible').count() == count
        page.locator('#proposal-search').blur()
        page.screenshot(path=str(ROOT / 'data/verification' / ('live-ledger-%s.png' % width)), full_page=True, animations='disabled')
        for path in pages:
            response = page.goto(base + path, wait_until='load')
            assert response.status == 200, path
            overflow = page.evaluate('document.documentElement.scrollWidth > innerWidth')
            results.append({'width': width, 'page': path, 'no_document_overflow': not overflow})
            if path.startswith('proposals/'):
                assert 'Chain identity unverified' in page.locator('body').inner_text()
                assert 'per cycle at' in page.locator('body').inner_text()
            if path == 'history.html':
                assert '6 / 6' in page.locator('body').inner_text()
                assert 'Work period ongoing' in page.locator('body').inner_text()
                page.screenshot(path=str(ROOT / 'data/verification' / ('live-history-%s.png' % width)), full_page=False, animations='disabled')
        page.close()
    browser.close()
assert not errors, errors
assert all(r['no_document_overflow'] for r in results), [r for r in results if not r['no_document_overflow']]

class Links(HTMLParser):
    def __init__(self):
        super().__init__(); self.links = []; self.ids = set()
    def handle_starttag(self, tag, attrs):
        values = dict(attrs)
        if values.get('id'): self.ids.add(values['id'])
        self.links.extend(values[k] for k in ('href', 'src') if values.get(k))

local_links = 0
for file in (ROOT / 'dist').rglob('*.html'):
    parser = Links(); parser.feed(file.read_text())
    for href in parser.links:
        url = urlsplit(href)
        if url.scheme or url.netloc: continue
        target = file.parent / unquote(url.path) if url.path else file
        assert target.is_file(), (file, href)
        if url.fragment and target.suffix == '.html':
            other = Links(); other.feed(target.read_text()); assert unquote(url.fragment) in other.ids, (file, href)
        local_links += 1
assert not (ROOT / 'dist/governance/latest.json').exists()
from datetime import datetime, timezone
record = {'checked_at': datetime.now(timezone.utc).isoformat(), 'browser': 'Google Chrome via Playwright',
          'checks': results, 'page_errors': errors, 'full_schema': 'Draft 2020-12 passed',
          'local_link_targets_and_anchors': local_links, 'search': 'six rows, two author matches, zero matches, reset',
          'reviewed_latest_absent': True}
(ROOT / 'data/verification/live-browser-checks.json').write_text(json.dumps(record, indent=2) + '\n')
print('Passed independent schema validation, %s browser page/viewport checks and %s local links/anchors.' % (len(results), local_links))
