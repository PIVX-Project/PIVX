import copy
import json
import tempfile
import time
import unittest
from datetime import datetime, timedelta, timezone
from html.parser import HTMLParser
from pathlib import Path
from unittest.mock import patch
from urllib.parse import parse_qs, unquote, urlsplit

from analyst import archive, contract, review, site
from analyst.contract import validate_report, validate_schema
from analyst.core import (CYCLE_BLOCKS, MAINNET_GENESIS, ROOT, DataError, active_for_cycle,
                          digest, money, next_superblock, normalize_proposal, public_url,
                          read_json, timestamp, utcnow, write_json)
from analyst.ingest import collect
from analyst.history import validate_history
from analyst.net import PROXIES, PublicClient, Redirect
from analyst.backfill import collect_cycles, payout_output
from analyst.sources import ForumCollector, ForumParser, get_price


def row(name="funded", char="a", *, failed=False):
    return {"Name": name, "URL": "https://forum.pivx.org/threads/example.1/", "Hash": char * 64, "FeeHash": "f" * 64,
            "BlockStart": CYCLE_BLOCKS * 130, "BlockEnd": CYCLE_BLOCKS * 133 - 1,
            "TotalPaymentCount": 3, "RemainingPaymentCount": 2, "PaymentAddress": "DpublicAddress",
            "Yeas": 0 if failed else 1, "Nays": 2 if failed else 0, "Abstains": 0,
            "MonthlyPayment": 1000.00000001, "TotalPayment": 3000.00000003,
            "IsValid": True, "IsEstablished": not failed, "Allotted": 0 if failed else 1000.00000001}


class FakeClock:
    def __init__(self):
        self.value = 0.0
        self.starts = []

    def now(self):
        return self.value

    def sleep(self, seconds):
        self.value += seconds


class RpcFixture:
    def __init__(self, clock, *, down=(), stale=False, wrong_network=False, disagree=False, malformed=False, boundary=False, bad_votes=False):
        self.clock, self.down, self.stale = clock, down, stale
        self.wrong_network, self.disagree, self.malformed = wrong_network, disagree, malformed
        self.boundary, self.bad_votes = boundary, bad_votes
        self.counts = {}

    def __call__(self, url, timeout):
        self.clock.starts.append(self.clock.now())
        parsed = urlsplit(url)
        if any(url.startswith(base) for base in self.down):
            raise OSError("synthetic provider unavailable")
        method = parsed.path.rsplit("/", 1)[-1]
        params = parse_qs(parsed.query).get("params", [None])[0]
        height = CYCLE_BLOCKS * 130 + 5
        proposals = [row(), row("failing", "b", failed=True)]
        if method == "getblockcount":
            self.counts[parsed.netloc] = self.counts.get(parsed.netloc, 0) + 1
            result = height + (CYCLE_BLOCKS if self.boundary and self.counts[parsed.netloc] > 1 else 0)
        elif method == "getbudgetinfo":
            if self.malformed:
                return 200, {}, b"<html>challenge</html>"
            result = proposals[:1] if self.disagree and parsed.netloc == "rpc.pivxla.bz" else proposals
        elif method == "getbudgetprojection":
            result = proposals[:1]
        elif method == "getblockhash":
            result = ("0" * 64 if self.wrong_network else MAINNET_GENESIS) if params == "0" else "c" * 64
        elif method == "getblock":
            result = {"hash": "c" * 64, "height": height, "time": int(time.time()) - (7200 if self.stale else 0)}
        elif method == "getbudgetvotes":
            directions = ["NO", "NO"] if params == "failing" else ["YES"]
            result = [] if self.bad_votes else [{"mnId": "public-%s" % n, "nHash": str(n) * 64, "Vote": vote, "nTime": int(time.time()), "fValid": True} for n, vote in enumerate(directions)]
        else:
            raise AssertionError(method)
        return 200, {"Content-Type": "application/json"}, json.dumps({"result": result, "error": None}).encode()


def fixture_snapshot(root, **options):
    clock = FakeClock()
    client = PublicClient(root / "data", retries=0, fetch=RpcFixture(clock, **options), clock=clock.now, sleep=clock.sleep)
    return collect(client), clock


def fixture_treasury(root, snapshot, *, omit_mirror=True, disagree=False, extra_payment=False):
    clock = FakeClock()
    cycles = range(snapshot["cycle"]["id"] - 6, snapshot["cycle"]["id"])
    def fetch(url, timeout):
        if url.endswith("/analytics/treasury"):
            rows = [{"height": n*CYCLE_BLOCKS, "total_paid": "1000"} for n in cycles]
            if "computingcache" in url:
                if disagree: rows[0]["total_paid"] = "999"
                elif omit_mirror: rows = rows[:-1]
            return 200, {}, json.dumps(rows).encode()
        height = int(url.rsplit("/", 1)[-1])
        value = 1000*100_000_000 if height % CYCLE_BLOCKS == 0 or extra_payment else 6*100_000_000
        block = {"height": height, "hash": str(height).zfill(64), "time": 1700000000 + height,
                 "tx": [{"tx_type": "coinstake", "txid": "d"*64, "vout": [
                     {"n": 0, "value": 9999999999999, "addresses": ["stake-principal"]},
                     {"n": 1, "value": value, "addresses": ["public-destination"]}]}]}
        return 200, {}, json.dumps(block).encode()
    client = PublicClient(root/"data", fetch=fetch, clock=clock.now, sleep=clock.sleep)
    return collect_cycles(client, snapshot)


class TreasuryTests(unittest.TestCase):
    def test_missing_index_requires_matching_payment_blocks_and_terminal_reward(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); snapshot,_=fixture_snapshot(root)
            treasury=fixture_treasury(root,snapshot)
            self.assertEqual(treasury["status"],"verified")
            self.assertEqual(len(treasury["cycles"]),6)
            self.assertEqual(len(treasury["index_disagreements"]),1)
            self.assertEqual(treasury["cycles"][0]["payments"][0]["piv"],"1000.00000000")
            self.assertTrue(all(c["payment_window_end_verified"] for c in treasury["cycles"]))
            for options in ({"disagree":True},{"extra_payment":True}):
                # Separate cache roots ensure the altered response is actually exercised.
                altered=root/str(len(list(root.iterdir())));altered.mkdir()
                with self.assertRaises(DataError):fixture_treasury(altered,snapshot,**options)

    def test_only_final_coinstake_output_and_integer_satoshis_are_accepted(self):
        block={"tx":[{"tx_type":"coinstake","txid":"a"*64,"vout":[
            {"n":0,"value":900000000000,"addresses":["principal"]},
            {"n":1,"value":1505000000000,"addresses":["budget"]}]}]}
        self.assertEqual(payout_output(block,"15050")["address"],"budget")
        for value in [15050,"1505000000000.5"]:
            changed=copy.deepcopy(block);changed["tx"][0]["vout"][-1]["value"]=value
            with self.assertRaises(DataError):payout_output(changed,"15050")


class CoreTests(unittest.TestCase):
    def test_strict_next_boundary_and_other_network_cycle(self):
        for height, expected in [(43199,43200),(43200,86400),(43201,86400),(0,43200)]:
            self.assertEqual(next_superblock(height),expected)
        self.assertEqual(next_superblock(144,144),288)
        for value in [-1, 1.5, True]:
            with self.assertRaises(DataError): next_superblock(value)

    def test_exact_decimal_and_invalid_amounts(self):
        self.assertEqual(money("1000.00000001"),"1000.00000001")
        for value in ["NaN", "Infinity", -1, True, "0.000000001"]:
            with self.assertRaises(DataError): money(value)

    def test_active_includes_failing_and_unestablished(self):
        proposal=normalize_proposal(row(failed=True))
        self.assertTrue(active_for_cycle(proposal,CYCLE_BLOCKS*131))
        self.assertFalse(active_for_cycle(proposal,CYCLE_BLOCKS*133))

    def test_structural_and_term_mismatches(self):
        for field,value in [("Yeas",True),("TotalPayment",1),("Hash","bad"),("IsValid","true"),("URL","http://example.com")]:
            item=row();item[field]=value
            with self.assertRaises(DataError): normalize_proposal(item)

    def test_public_destinations(self):
        for url in ["http://example.com","https://localhost/","https://127.0.0.1/","https://192.168.1.1/","https://user:password@example.com","https://example.com:123/"]:
            with self.assertRaises(DataError): public_url(url)
        self.assertEqual(public_url("https://forum.pivx.org/"),"https://forum.pivx.org/")


class IngestTests(unittest.TestCase):
    def test_redirects_are_throttled_public_and_bounded(self):
        with tempfile.TemporaryDirectory() as directory:
            clock=FakeClock();calls=[]
            def fetch(url, timeout):
                calls.append(clock.now())
                if url.endswith('/start'):raise Redirect('https://example.com/end')
                if url.endswith('/private'):raise Redirect('https://127.0.0.1/')
                if url.endswith('/loop'):raise Redirect(url)
                return 200,{},b'public response'
            client=PublicClient(directory,fetch=fetch,retries=0,clock=clock.now,sleep=clock.sleep)
            body,meta=client.get('https://example.com/start')
            self.assertEqual(body,b'public response')
            self.assertEqual(meta['url'],'https://example.com/end')
            self.assertGreaterEqual(calls[1]-calls[0],1)
            for path in ('private','loop'):
                with self.assertRaises(DataError):client.get('https://example.com/'+path)
    def test_full_run_retains_raw_and_failing_roster_with_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            snapshot,clock=fixture_snapshot(root,down=(PROXIES[0],))
            self.assertEqual(snapshot["status"],"verified")
            self.assertEqual(set(snapshot["active_ids"]),{"a"*64,"b"*64})
            self.assertEqual(snapshot["projected_ids"],["a"*64])
            self.assertEqual(len(snapshot["vote_details"]),2)
            self.assertNotEqual(snapshot["selected_endpoint"],PROXIES[0])
            self.assertTrue(list((root/"data/raw").glob("*.body")))
            self.assertTrue(all(b-a>=1 for a,b in zip(clock.starts,clock.starts[1:])))

    def test_no_provider_is_explicitly_blocked(self):
        with tempfile.TemporaryDirectory() as directory:
            snapshot,_=fixture_snapshot(Path(directory),down=PROXIES)
            self.assertEqual(snapshot["status"],"blocked")
            self.assertIsNone(snapshot["cycle"])
            self.assertTrue(all(p["errors"] for p in snapshot["providers"]))

    def test_wrong_network_stale_disagreement_and_bad_votes_never_verify(self):
        for options in [dict(wrong_network=True),dict(stale=True),dict(disagree=True),dict(bad_votes=True)]:
            with self.subTest(options=options), tempfile.TemporaryDirectory() as directory:
                snapshot,_=fixture_snapshot(Path(directory),**options)
                self.assertEqual(snapshot["status"],"partial")
                self.assertTrue(snapshot["issues"])

    def test_http_200_challenge_and_boundary_change_are_blocked(self):
        for options in [dict(malformed=True),dict(boundary=True)]:
            with tempfile.TemporaryDirectory() as directory:
                snapshot,_=fixture_snapshot(Path(directory),**options)
                self.assertEqual(snapshot["status"],"blocked")

    def test_cache_preserves_original_timestamp_and_avoids_network(self):
        with tempfile.TemporaryDirectory() as directory:
            clock=FakeClock();fixture=RpcFixture(clock)
            client=PublicClient(Path(directory),fetch=fixture,clock=clock.now,sleep=clock.sleep)
            url=PROXIES[0]+"/getblockcount"
            first,meta=client.get(url)
            second,meta2=client.get(url)
            self.assertEqual(first,second);self.assertEqual(meta,meta2)
            self.assertEqual(len(clock.starts),1)

    def test_write_methods_rejected_before_fetch(self):
        with tempfile.TemporaryDirectory() as directory:
            client=PublicClient(directory,fetch=lambda *_: self.fail("Network must not be called"))
            with self.assertRaises(DataError): client.rpc(PROXIES[0],"mnbudgetvote","yes")


class SourceTests(unittest.TestCase):
    def test_xhtml_breaks_quotes_and_edit_date_do_not_truncate_or_rewrite_posts(self):
        parser=ForumParser()
        parser.feed('''<article class="message" data-author="A" data-content="post-1"><time datetime="2026-06-01T00:00:00Z"></time><div class="bbWrapper">Name: Original<br/>Address: Dpublic<br/><blockquote>Name: Quoted<br/>Cycle Amount: 15,000</blockquote><br/>Tail survives</div><time datetime="2026-10-01T00:00:00Z"></time></article>''')
        post=parser.result()[0]
        self.assertIn('Tail survives',post['text'])
        self.assertEqual(post['fields']['name'],'Original')
        self.assertEqual(post['quoted_fields']['name'],'Quoted')
        self.assertEqual(post['quoted_fields']['cycle amount'],'15,000')
        self.assertEqual(post['posted_at'],'2026-06-01T00:00:00Z')
    def test_posts_authors_dates_quotes_and_pages_stay_distinct(self):
        parser=ForumParser()
        parser.feed('<article class="message message--post" data-author="A" data-content="post-1"><time datetime="2026-01-01T00:00:00Z"></time><div class="bbWrapper">Name: First<br>Term: 3 Cycles<blockquote>Old claim</blockquote><a href="https://github.com/example/repo">Evidence</a></div></article><article class="message" data-author="B" data-content="post-2"><time datetime="2026-02-01T00:00:00Z"></time><div class="bbWrapper">Follow-up</div></article><a class="pageNav-jump--next" href="/threads/example.1/page-2">Next</a>')
        posts=parser.result()
        self.assertEqual([p["author"] for p in posts],["A","B"])
        self.assertNotEqual(posts[0]["posted_at"],posts[1]["posted_at"])
        self.assertNotIn("Old claim",posts[0]["text"])
        self.assertIn("Old claim",posts[0]["quoted_text"])
        self.assertEqual(posts[0]["fields"]["name"],"First")
        self.assertIn("page-2",parser.next_page)

    def test_robots_denied_means_no_thread_fetch(self):
        calls=[]
        def fetch(url,timeout):
            calls.append(url)
            return 200,{},b'User-agent: *\nDisallow: /threads/\n'
        with tempfile.TemporaryDirectory() as directory:
            client=PublicClient(directory,fetch=fetch)
            with self.assertRaises(DataError): ForumCollector(client).thread("https://forum.pivx.org/threads/example.1/")
            self.assertEqual(len(calls),1)

    def test_stale_or_missing_price_is_unknown(self):
        for data in [{},{"pivx":{"usd":0.05,"last_updated_at":1}},{"pivx":{"usd":0,"last_updated_at":int(time.time())}},{"pivx":{"usd":"invalid","last_updated_at":int(time.time())}}]:
            class Client:
                def json(self,*args,**kwargs): return data,{}
            with self.assertRaises(DataError): get_price(Client())


class ReportTests(unittest.TestCase):
    def setUp(self):
        self.report=read_json(ROOT/"data/drafts/initial-report.json")

    def test_synthetic_drafts_validate_without_chain_or_human_approval(self):
        self.assertTrue(validate_report(self.report))
        self.assertEqual(len(self.report["proposals"]),6)
        self.assertIsNone(self.report["coverage"]["expected_active_ids"])
        self.assertEqual(self.report['status'],'draft')
        self.assertTrue(all(not p['chain_verified'] for p in self.report['proposals']))

    def test_oversized_summary_unknown_sources_and_coverage_fail(self):
        for mutation in ("summary","source","coverage","score","cost"):
            report=copy.deepcopy(self.report)
            if mutation=="summary":report["proposals"][0]["summary"]["text"]="word "*151
            if mutation=="source":report["proposals"][0]["summary"]["source_ids"]=["invented"]
            if mutation=="coverage":report["coverage"]["analyzed_ids"]=[]
            if mutation=="score":report["proposals"][0]["scores"]["clarity"]["value"]=True
            if mutation=="cost":report["proposals"][0]["cost"]["term_piv"]="1.00000000"
            with self.subTest(mutation=mutation),self.assertRaises(DataError):validate_report(report)

    def test_unknown_schema_vocabulary_fails_closed(self):
        with self.assertRaises(DataError):validate_schema("x",{"type":"string","madeUp":True})

    def test_preview_cannot_emit_reviewed_endpoints_or_overwrite_unowned_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory)/"preview"
            site.build_site(self.report,out)
            self.assertTrue((out/"governance/draft.json").is_file())
            self.assertFalse((out/"governance/latest.json").exists())
            self.assertIn("noindex,nofollow",(out/"index.html").read_text())
            self.assertEqual(len(list((out/"proposals").glob("*.html"))),6)
            class Links(HTMLParser):
                def __init__(self):super().__init__();self.links=[]
                def handle_starttag(self,tag,attrs):
                    values=dict(attrs)
                    self.links.extend(values[k] for k in ("href","src") if values.get(k))
            for page in out.rglob("*.html"):
                links=Links();links.feed(page.read_text())
                for link in links.links:
                    parsed=urlsplit(link)
                    if not parsed.scheme and not parsed.netloc and parsed.path:
                        self.assertTrue((page.parent/unquote(parsed.path)).is_file(),str(page)+": "+link)
            with self.assertRaises(DataError):site.build_site(self.report,Path(directory),published=True)
            other=Path(directory)/"unowned";other.mkdir();(other/"keep.txt").write_text("preserve me")
            with self.assertRaises(DataError):site.build_site(self.report,other)
            self.assertTrue((other/"keep.txt").exists())

    def test_draft_export_denied_for_multiple_independent_reasons(self):
        errors=review.publication_errors(self.report)
        self.assertTrue(any("disclosure" in e for e in errors))
        self.assertTrue(any("not marked reviewed" in e for e in errors))
        self.assertTrue(any("review records" in e for e in errors))

    def test_pending_review_template_is_denied_cleanly(self):
        reviews={"package_sha256":"missing","correction_window":{"public_invitation_url":None,"opened_at":None,"closed_at":None},
                 "proposals":[{"proposal_id":p["id"],"reviewer":None,"decision":"pending","reviewed_at":None} for p in self.report["proposals"]]}
        errors=review.publication_errors(self.report,reviews)
        self.assertTrue(any("48-hour" in e for e in errors))
        self.assertTrue(any("review timestamp" in e for e in errors))

    def test_complete_fixture_export_and_two_cycle_archive(self):
        # Synthetic local integration only. Never creates review records for real proposals.
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for name in ("RUBRIC.md","NEUTRALITY.md","governance.schema.json"):
                (root/name).write_text((ROOT/name).read_text())
            for name in (self.report["generation"]["input_path"], self.report["generation"]["prompt_path"]):
                target=root/name;target.parent.mkdir(parents=True,exist_ok=True)
                target.write_text((ROOT/name).read_text())
            snapshot,_=fixture_snapshot(root)
            path="data/runs/fixture/snapshot.json";write_json(root/path,snapshot)
            report=copy.deepcopy(self.report)
            synthetic_inputs={"kind":"synthetic_test_inputs"}
            write_json(root/report['generation']['input_path'],synthetic_inputs)
            report['generation']['input_sha256']=digest(synthetic_inputs)
            report.update(status="reviewed",cycle={k:snapshot["cycle"][k] for k in ("id","payout_height")})
            report["snapshot"]={"status":"verified","path":path,"content_sha256":snapshot["content_sha256"]}
            report["disclosures"]={"status":"confirmed","text":"Synthetic test disclosure, not Michael's real statement."}
            report["proposals"]=report["proposals"][:2]
            for n,p in enumerate(report["proposals"]):
                p.update(id=("a" if n==0 else "b")*64,chain_hash=("a" if n==0 else "b")*64,chain_verified=True)
                p["history"]["status"]="researched"
                normalized=snapshot["proposals"][n]
                p["name"]=normalized["name"]
                for key in ("cycle_piv","term_piv","payment_count"):p["cost"][key]=normalized[key]
                p['cost']['usd']=None
            active=snapshot["active_ids"]
            report["coverage"]={"basis":"Synthetic verified test roster","expected_active_ids":active,"analyzed_ids":active}
            cycle=report["cycle"]["id"]
            past=list(range(cycle-6,cycle))
            treasury=fixture_treasury(root,snapshot)
            write_json(root/'data/research/test-treasury.json',treasury)
            aliases=sorted({p['proposer'] for p in report['proposals']})
            history={"status":"complete","sources":[{"id":"fixture","url":"https://example.com/public-test-evidence"}],
                     "search_scope":{"thread_ids":["fixture"],"discovery_source_ids":["fixture"]},
                     "treasury_artifact":{"path":"data/research/test-treasury.json","content_sha256":treasury['content_sha256']},
                     "cycles":[{"id":n,"status":"complete","payout_date":"2026-01-01T00:00:00Z","payment_count":1,"total_paid_piv":"1000","period_finished":True,"evidence_urls":["https://example.com/public-test-evidence"],
                                "searches":[{"proposer":name,"searched_source_ids":["fixture"],"delivery_result":"unknown"} for name in aliases]} for n in past],
                     "proposers":[{"name":name,"complete_cycle_ids":past} for name in aliases]}
            write_json(root/"data/proposers.json",history)
            with patch.object(contract,"ROOT",root),patch.object(review,"ROOT",root),patch.object(archive,"ROOT",root):
                package_hash=contract.review_digest(report)
                now=datetime.now(timezone.utc)
                reviews={"package_sha256":package_hash,"correction_window":{"public_invitation_url":"https://example.com/public-test-invitation","opened_at":(now-timedelta(hours=73)).isoformat(),"closed_at":(now-timedelta(hours=24)).isoformat()},
                         "proposals":[{"proposal_id":p["id"],"reviewer":"Michael Korovkin","decision":"approved","reviewed_at":(now-timedelta(hours=1)).isoformat(),"package_sha256":package_hash} for p in report["proposals"]],
                         "evidence_checks":[{"source_id":s["id"],"result":"checked"} for s in report["sources"]]}
                self.assertEqual(review.publication_errors(report,reviews),[])
                package=archive.package_report(report,reviews)
                archive.retain_reviewed(package)
                self.assertEqual(len(archive.reviewed_packages()),1)
                # Second archived cycle uses an already-reviewed package with its original documents.
                previous=copy.deepcopy(package)
                previous["report"]["cycle"]={"id":cycle-1,"payout_height":(cycle-1)*CYCLE_BLOCKS}
                previous_hash=digest({"report":previous["report"],"documents":previous["documents"]})
                previous["reviews"]["package_sha256"]=previous_hash
                for r in previous["reviews"]["proposals"]:r["package_sha256"]=previous_hash
                out=root/"site"
                site.build_site(report,out,published=True,reviews=reviews,archives=[previous])
                self.assertTrue((out/"governance/latest.json").exists())
                self.assertTrue((out/("cycles/%s/index.html"%(cycle-1))).exists())
                self.assertTrue((out/("cycles/%s/history.html"%(cycle-1))).exists())
                self.assertTrue((out/("governance/cycle-%s.json"%(cycle-1))).exists())
                # Any editorial or raw snapshot edit invalidates a prior approval.
                changed=copy.deepcopy(report);changed["proposals"][0]["summary"]["text"]+=" Edited."
                self.assertTrue(any("hash" in e for e in review.publication_errors(changed,reviews)))
                first=next((root/"data/raw").glob("*.body"));first.write_text("tampered")
                self.assertTrue(any("checksum" in e for e in review.publication_errors(report,reviews)))


class HistoryTests(unittest.TestCase):
    def setUp(self):
        self.history=read_json(ROOT/"data/proposers.json")

    def test_six_public_payout_period_searches_keep_delivery_unknown(self):
        self.assertTrue(validate_history(self.history))
        self.assertEqual(self.history["status"],"complete")
        self.assertEqual(len(self.history["cycles"]),6)
        self.assertFalse(self.history['delivery_evidence_complete'])
        self.assertFalse(self.history['cycles'][-1]['period_finished'])
        self.assertTrue(all(s['delivery_result']=='unknown' for c in self.history['cycles'] for s in c['searches']))

    def test_empty_cycle_labels_are_not_completed_research(self):
        for mutation in ('scope','searches','receipt'):
            history=copy.deepcopy(self.history)
            if mutation=='scope':history['search_scope']={}
            if mutation=='searches':history['cycles'][0]['searches']=[]
            if mutation=='receipt':history['cycles'][0]['searches'][0]['matched_payment_event_ids']=['invented']
            with self.subTest(mutation=mutation),self.assertRaises(DataError):validate_history(history)

    def test_wrong_alias_and_unknown_sources_are_rejected(self):
        for mutation in ("alias","citation"):
            history=copy.deepcopy(self.history)
            record=next(p["timeline"][0] for p in history["proposers"] if p.get("timeline"))
            if mutation=="alias":record["proposer"]="unrelated alias"
            else:record["source_ids"]=["nonexistent"]
            with self.subTest(mutation=mutation),self.assertRaises(DataError):validate_history(history)

    def test_reported_work_period_does_not_replace_post_date(self):
        record=next(r for p in self.history["proposers"] for r in p.get("timeline",[]) if r["id"]=="reported-work-example")
        self.assertEqual(record["date"],"2026-06-22")
        self.assertEqual(record["reported_period"],"2026-05")
        rendered=site.history_body(self.history)
        self.assertIn("2026-06-22",rendered)
        self.assertIn("work period 2026-05",rendered)
        self.assertIn("6 / 6",rendered)


if __name__=="__main__":unittest.main()
