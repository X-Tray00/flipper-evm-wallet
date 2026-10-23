(() => {
  if (window !== window.top) return;
  const topic = 'flipper-evm-wallet-v1';
  let active = 0;
  window.addEventListener('message', async (e) => {
    if (
      e.source !== window ||
      e.origin !== location.origin ||
      e.data?.topic !== topic ||
      e.data.direction !== 'request'
    )
      return;
    const { id, method, params } = e.data;
    if (
      typeof id !== 'string' ||
      id.length > 100 ||
      typeof method !== 'string' ||
      method.length > 80
    )
      return;
    const response = (data: object) =>
      window.postMessage({ topic, direction: 'response', id, ...data }, location.origin);
    if (active >= 4) {
      response({ error: { code: -32002, message: 'Too many requests' } });
      return;
    }
    try {
      if (JSON.stringify(params).length > 24000) throw new Error('Request too large');
      active++;
      const result = await chrome.runtime.sendMessage({ kind: 'rpc', method, params });
      response(result);
    } catch (e) {
      response({
        error: { code: 4900, message: e instanceof Error ? e.message : 'Wallet unavailable' },
      });
    } finally {
      active = Math.max(0, active - 1);
    }
  });
  chrome.runtime.onMessage.addListener((m, _sender, respond) => {
    if (m?.kind === 'ping') {
      respond({ alive: true });
      return;
    }
    if (m?.kind === 'originEvent' && m.origin === location.origin)
      window.postMessage(
        { topic, direction: 'response', event: m.event, value: m.value },
        location.origin,
      );
  });
})();
