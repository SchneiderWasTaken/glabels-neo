// A Durable Object that makes the single-use challenge check and the rate limits ATOMIC.
//
// Cloudflare KV is eventually consistent and has no atomic increment, so a burst of concurrent requests could all see
// "not used yet" and "counter still 0".  A Durable Object processes one request at a time, so a nonce can be claimed
// once and the counters cannot be raced.  One instance (named "global") serves the whole relay: reports are rare.

const HOUR = 3600_000;
const DAY = 86_400_000;

export class Limiter {
  constructor(state) {
    this.state = state;
  }

  async fetch(request) {
    const { nonce, nonceExpires, reporter, perHour, perDay, global, now } = await request.json();
    const store = this.state.storage;

    const hourKey = `h:${reporter}:${Math.floor(now / HOUR)}`;
    const dayKey = `d:${reporter}:${Math.floor(now / DAY)}`;
    const globalKey = `g:${Math.floor(now / DAY)}`;
    const nonceKey = `n:${nonce}`;

    const [used, h, d, g] = await Promise.all([store.get(nonceKey), store.get(hourKey), store.get(dayKey), store.get(globalKey)]);
    const reply = (body, status = 200) => new Response(JSON.stringify(body), { status, headers: { 'content-type': 'application/json' } });

    if (used) return reply({ ok: false, kind: 'nonce' });
    if ((g ?? 0) >= global) return reply({ ok: false, kind: 'limit', scope: 'global', retryAfter: 3600 });
    if ((h ?? 0) >= perHour) return reply({ ok: false, kind: 'limit', scope: 'reporter', retryAfter: 3600 - Math.floor((now % HOUR) / 1000) });
    if ((d ?? 0) >= perDay) return reply({ ok: false, kind: 'limit', scope: 'reporter', retryAfter: 86400 - Math.floor((now % DAY) / 1000) });

    await store.put({ [nonceKey]: nonceExpires, [hourKey]: (h ?? 0) + 1, [dayKey]: (d ?? 0) + 1, [globalKey]: (g ?? 0) + 1 });

    // Everything here is only meaningful for a day: wipe the table daily so it cannot grow without bound.
    if ((await store.getAlarm?.()) == null) await store.setAlarm?.(now + DAY + 60_000);
    return reply({ ok: true });
  }

  async alarm() {
    await this.state.storage.deleteAll();
  }
}
