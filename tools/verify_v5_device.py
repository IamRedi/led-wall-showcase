#!/usr/bin/env python3
"""Bounded V5 API smoke test on the already identified ESP32.

Run after app readback verification. No filesystem mutation or high-power frame.
"""
import hashlib
import json
import sys
import time
import urllib.request
from pathlib import Path

BASE = 'http://192.168.1.90'
ROOT = Path(__file__).resolve().parents[1]
records = []


def request(path, body=None, raw=False):
    data = None if body is None else json.dumps(body, separators=(',', ':')).encode()
    req = urllib.request.Request(BASE + path, data=data,
                                 headers={'Content-Type': 'application/json'} if data else {})
    with urllib.request.urlopen(req, timeout=20) as response:
        result = response.read(100000)
        if response.status != 200:
            raise AssertionError(f'{path}: HTTP {response.status}')
    return result if raw else json.loads(result)


def check(label, condition, detail):
    if not condition:
        raise AssertionError(f'{label}: {detail}')
    records.append({'check': label, 'result': detail})
    print(label, detail, flush=True)


def post(path, body=None):
    result = request(path, {} if body is None else body)
    check(path, result.get('ok') is True, result)
    return result


def main():
    identity = request('/identity')
    check('firmware', identity.get('fwVersion') == '0.5.0-alpha.1', identity.get('fwVersion'))
    html = request('/', raw=True)
    expected = (ROOT / 'firmware/web/v5.html').read_text().replace(
        '/*CORE_PLACEHOLDER*/', (ROOT / 'firmware/web/ui_core.js').read_text()).encode()
    check('device UI exact bytes', html == expected,
          {'sha256': hashlib.sha256(html).hexdigest(), 'bytes': len(html)})
    registry = request('/registry')
    check('curated catalog', len(registry.get('publicScenes', [])) == 10 and
          len(registry.get('partyMoments', [])) == 6 and
          len(registry.get('games', [])) == 14,
          {'scenes': registry.get('publicScenes'), 'party': registry.get('partyMoments'),
           'gameIds': len(registry.get('games', []))})
    state = request('/state')
    check('physical configuration', state['matrix'] == {'width': 30, 'height': 10} and
          state['tail']['enabled'] and state['tail']['length'] == 150 and
          state['brightness']['max'] == 40,
          {'matrix': state['matrix'], 'tail': state['tail'], 'brightness': state['brightness']})
    post('/display/clear')
    post('/display/brightness/set', {'level': 5})
    state = request('/state')
    check('safe brightness', state['brightness'] == {'current': 5, 'max': 40}, state['brightness'])

    post('/display/animation/set', {'animation': 'fireworks', 'target': 'both'})
    state = request('/state')
    check('fireworks device state', state['activeAnimation'] == 'fireworks' and
          state['animationTarget'] == 'both',
          {'mode': state['currentMode'], 'target': state['animationTarget']})
    post('/display/clear')

    post('/display/game/difficulty', {'difficulty': 'easy'})
    post('/display/game/set', {'game': 'pong', 'players': 2})
    state = request('/state')
    check('two-player game launch', state['game']['active'] and
          state['game']['players'] == 2 and state['game']['name'] == 'pong', state['game'])
    p1 = post('/display/input/session', {'player': 1})
    p2 = post('/display/input/session', {'player': 2})
    check('independent input sessions', p1['session'] != p2['session'],
          {'p1': p1['player'], 'p2': p2['player']})
    post('/display/input/event', {'player': 1, 'session': p1['session'],
                                  'sequence': 1, 'input': 'confirm', 'state': 'press'})
    post('/display/input/event', {'player': 2, 'session': p2['session'],
                                  'sequence': 1, 'input': 'up', 'state': 'press'})
    post('/display/clear')
    state = request('/state')
    check('Clear exits game engine', state['currentMode'] == 'idle' and
          state['game']['active'] is False, state['game'])
    post('/display/input/release', {'player': 1, 'session': p1['session']})
    post('/display/input/release', {'player': 2, 'session': p2['session']})

    pixels = ['000000'] * 450
    pixels[0], pixels[300], pixels[449] = '000100', '000100', '000001'
    session = post('/display/pixel/session')
    ack = post('/display/pixel/frame', {'session': session['session'], 'revision': 1,
                                        'pixels': ''.join(pixels), 'live': False})
    check('pixel exact ACK', ack.get('session') == session['session'] and
          ack.get('revision') == 1 and ack.get('applied') is True, ack)
    post('/display/clear')

    post('/display/text/show', {'text': 'V5', 'mode': 'fixed', 'color': 'heritage_green',
                                'tailMode': 'solid', 'tailColor': 'heritage_green'})
    state = request('/state')
    check('text device state', state['currentMode'] == 'text' and
          state['text']['current'] == 'V5', state.get('text'))
    post('/display/clear')

    post('/display/timer/set', {'durationMs': 30000})
    post('/display/timer/start')
    post('/display/timer/pause')
    check('timer pause', request('/state')['timer']['state'] == 'paused',
          request('/state')['timer'])
    post('/display/timer/resume')
    post('/display/timer/reset')
    post('/display/timer/stop')
    check('timer stop', request('/state')['timer']['state'] == 'idle',
          request('/state')['timer'])

    post('/display/party/play', {'preset': 'welcome'})
    state = request('/state')
    check('party foreground', state['currentMode'] in ('text', 'scene') and
          state['game']['active'] is False,
          {'mode': state['currentMode'], 'game': state['game']['active']})
    post('/display/party/stop')
    post('/display/clear')
    state = request('/state')
    check('final safe state', state['currentMode'] == 'idle' and
          state['brightness'] == {'current': 5, 'max': 40} and
          state['game']['active'] is False,
          {'mode': state['currentMode'], 'brightness': state['brightness'],
           'uptimeSeconds': state['uptimeSeconds']})
    print(json.dumps({'ok': True, 'records': records}, separators=(',', ':')), flush=True)


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(f'FAIL {type(exc).__name__}: {exc}', file=sys.stderr, flush=True)
        try:
            post('/display/clear')
        except Exception as cleanup:
            print(f'cleanup failed: {cleanup}', file=sys.stderr, flush=True)
        raise
