"""Thin HTTPS/WebSocket transport. C++ owns search decisions, budgets and durable writes."""
import asyncio
import html
import json
import re
import uuid
from urllib.parse import urlencode, quote
from urllib.request import Request, build_opener, HTTPRedirectHandler
from ws_client import Client


class NoRedirects(HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, newurl):
        raise ValueError('search endpoint redirected; update the provider explicitly')


def search_wikipedia(query):
    if not isinstance(query, str) or not 0 < len(query.encode()) <= 512:
        raise ValueError('search query must be 1..512 bytes')
    url = 'https://en.wikipedia.org/w/rest.php/v1/search/page?' + urlencode({'q': query, 'limit': 3})
    request = Request(url, headers={'User-Agent': 'WDS-AI/0.8 (local research workshop)',
                                    'Accept': 'application/json'})
    with build_opener(NoRedirects()).open(request, timeout=20) as response:
        data = response.read(262145)
        if len(data) > 262144:
            raise ValueError('search response exceeded 256 KiB; no partial response accepted')
    pages = json.loads(data).get('pages', [])
    results = []
    for page in pages[:3]:
        title = page.get('title', '')
        text = html.unescape(re.sub(r'<[^>]*>', '', page.get('excerpt', ''))).strip()
        description = page.get('description') or ''
        text = (description + '\n' + text).strip()
        source = 'https://en.wikipedia.org/wiki/' + quote(page.get('key', ''), safe='')
        # Never quietly cut a source into something that looks complete.
        if not text or len(text.encode()) > 1800 or len(title.encode()) > 256 or len(source.encode()) > 256:
            continue
        results.append({'url': source, 'title': title, 'text': text, 'excerpt': True})
    if not results:
        raise ValueError('no usable search excerpts; provide source material or revise the query')
    return results


async def complete(endpoint, item, searcher=search_wikipedia):
    async with Client(endpoint) as rpc:
        claim = uuid.uuid4().hex
        try:
            assigned = await rpc.call('mesh.web.claim', 'mesh.web_assigned', request=claim,
                                      root_id=item['root_id'], web_key=item['key'])
        except RuntimeError as error:
            print(json.dumps({'type': 'mesh.web_claim_rejected', 'key': item['key'], 'reason': str(error)}), flush=True)
            return
        item = assigned['data']['request']
        try:
            if item['provider'] != 'wikipedia':
                raise ValueError('unsupported search provider')
            results = await asyncio.wait_for(asyncio.to_thread(searcher, item['query']), 25)
            payload = {'results': results}
        except Exception as error:
            payload = {'error': (str(error) or type(error).__name__)[:900]}
        try:
            result = await rpc.call('mesh.web.complete', 'mesh.web_received', root_id=item['root_id'],
                                    web_key=item['key'], claim_id=claim, **payload)
            print(json.dumps({'type': 'mesh.web_result', 'key': item['key'], 'status': result['data']['status']}), flush=True)
        except RuntimeError as error:
            # An expired lease is already durably held by the coordinator.
            print(json.dumps({'type': 'mesh.web_delivery_failed', 'key': item['key'], 'reason': str(error)}), flush=True)


async def run(endpoint, searcher=search_wikipedia):
    async with Client(endpoint) as watch:
        await watch.call('upkeep.watch', 'upkeep.snapshot', root_id='*')
        async with Client(endpoint) as rpc:
            snapshot = (await rpc.call('mesh.inspect', 'mesh.snapshot', summary=True))['data']
        enabled = snapshot['settings']['enabled'] and snapshot['settings']['web_enabled']
        pending = {w['key']: w for w in snapshot['pending_web'] if w['status'] == 'pending'} if enabled else {}
        while True:
            while pending:
                _, item = pending.popitem()
                await complete(endpoint, item, searcher)
            event = json.loads(await watch.ws.recv())
            if event['type'] == 'error':
                raise RuntimeError(event)
            if event['type'] == 'mesh.impulse':
                for item in event['data'].get('mesh', {}).get('web', []):
                    if item['status'] == 'pending':
                        pending[item['key']] = item
            elif event['type'] == 'mesh.configured':
                config = event['data']['settings']
                if config['enabled'] and config['web_enabled']:
                    async with Client(endpoint) as rpc:
                        snapshot = (await rpc.call('mesh.inspect', 'mesh.snapshot', summary=True))['data']
                    pending.update({w['key']: w for w in snapshot['pending_web'] if w['status'] == 'pending'})


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--endpoint', default='ws://127.0.0.1:8765')
    asyncio.run(run(parser.parse_args().endpoint))
