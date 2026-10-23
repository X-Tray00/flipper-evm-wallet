(() => {
  const topic = 'flipper-evm-wallet-v1';
  let sequence = 0;
  const pending = new Map<
    string,
    {
      resolve: (v: unknown) => void;
      reject: (e: unknown) => void;
      timer: ReturnType<typeof setTimeout>;
    }
  >();
  const handlers = new Map<string, Set<(value: unknown) => void>>();
  const emit = (name: string, value: unknown) => {
    for (const fn of handlers.get(name) || []) {
      try {
        fn(value);
      } catch {
        /* A site listener must not break the provider. */
      }
    }
  };
  window.addEventListener('message', (e) => {
    if (
      e.source !== window ||
      e.origin !== location.origin ||
      e.data?.topic !== topic ||
      e.data.direction !== 'response'
    )
      return;
    if (e.data.event) {
      if (['accountsChanged', 'chainChanged', 'connect', 'disconnect'].includes(e.data.event))
        emit(e.data.event, e.data.value);
      return;
    }
    const p = pending.get(e.data.id);
    if (!p) return;
    clearTimeout(p.timer);
    pending.delete(e.data.id);
    if (e.data.error)
      p.reject(Object.assign(new Error(e.data.error.message), { code: e.data.error.code }));
    else p.resolve(e.data.result);
  });
  const provider = Object.freeze({
    isFlipperWallet: true,
    request(args: { method: string; params?: unknown[] | object }): Promise<unknown> {
      if (!args || typeof args.method !== 'string' || args.method.length > 80)
        return Promise.reject(Object.assign(new Error('Invalid request'), { code: -32602 }));
      const id = `${++sequence}-${crypto.randomUUID()}`;
      return new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
          pending.delete(id);
          reject(Object.assign(new Error('Wallet request timed out'), { code: 4001 }));
        }, 240000);
        pending.set(id, { resolve, reject, timer });
        window.postMessage(
          { topic, direction: 'request', id, method: args.method, params: args.params ?? [] },
          location.origin,
        );
      });
    },
    on(event: string, fn: (value: unknown) => void) {
      if (!handlers.has(event)) handlers.set(event, new Set());
      handlers.get(event)!.add(fn);
      return provider;
    },
    removeListener(event: string, fn: (value: unknown) => void) {
      handlers.get(event)?.delete(fn);
      return provider;
    },
  });
  const icon =
    'data:image/svg+xml,' +
    encodeURIComponent(
      '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="16" fill="#142b26"/><path d="M18 16h30v8H26v8h18v8H26v12h-8z" fill="#a2f3bc"/></svg>',
    );
  const detail = Object.freeze({
    info: Object.freeze({
      uuid: crypto.randomUUID(),
      name: 'Flipper EVM Wallet',
      icon,
      rdns: 'io.github.x-tray00.flipper-evm-wallet',
    }),
    provider,
  });
  const announce = () =>
    window.dispatchEvent(new CustomEvent('eip6963:announceProvider', { detail }));
  window.addEventListener('eip6963:requestProvider', announce);
  announce();
  // Do not overwrite another provider. EIP-6963 discovery always remains available.
  if (!('ethereum' in window))
    Object.defineProperty(window, 'ethereum', {
      value: provider,
      writable: true,
      configurable: true,
    });
})();
