import React, { useEffect, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { formatEther, parseEther, toHex, encodeFunctionData, type Address } from 'viem';
import { CHAINS } from './policy';
import { FILTER } from './hid';
import './wallet.css';
import { DEFAULT_LOCALE, getLocale, languages } from '../../shared/locales';
import { translations, type RequestTitle } from '../../shared/messages';
async function call(action: string, args: object = {}) {
  if (!globalThis.chrome?.runtime?.id)
    throw new Error('Open this page from the installed extension');
  const r = await chrome.runtime.sendMessage({ kind: 'ui', action, ...args });
  if (r.error) throw new Error(r.error.message);
  return r.result;
}
function App() {
  const [snapshot, setSnapshot] = useState<any>({
    state: {
      chain: 11155111,
      account: 0,
      locale: DEFAULT_LOCALE,
      addresses: {},
      grants: {},
      assets: [],
      history: [],
    },
    connected: false,
    pending: null,
  });
  const [tab, setTab] = useState('home'),
    [error, setError] = useState(''),
    [message, setMessage] = useState(''),
    [busy, setBusy] = useState(false),
    [balance, setBalance] = useState<string | null>(null);
  const [operation, setOperation] = useState('ETH'),
    [to, setTo] = useState(''),
    [amount, setAmount] = useState(''),
    [contract, setContract] = useState(''),
    [token, setToken] = useState('');
  const [watchType, setWatchType] = useState('ERC20'),
    [watchAddress, setWatchAddress] = useState(''),
    [watchId, setWatchId] = useState('');
  const st = snapshot.state,
    locale = getLocale(st.locale),
    t = translations[locale],
    addr = st.addresses[st.account],
    chain = CHAINS[st.chain as keyof typeof CHAINS];
  async function refresh() {
    try {
      setSnapshot(await call('state'));
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }
  useEffect(() => {
    void refresh();
    const timer = setInterval(() => void refresh(), 1500);
    return () => clearInterval(timer);
  }, []);
  useEffect(() => {
    document.documentElement.lang = locale;
  }, [locale]);
  useEffect(() => {
    setBalance(null);
  }, [st.chain, st.account]);
  async function act(fn: () => Promise<unknown>) {
    setBusy(true);
    setError('');
    setMessage('');
    try {
      await fn();
      await refresh();
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  }
  const field = (label: string, value: string, set: (s: string) => void, placeholder = '') => (
    <label>
      {label}
      <input
        value={value}
        onChange={(e) => set(e.target.value)}
        placeholder={placeholder}
        autoComplete="off"
        spellCheck={false}
      />
    </label>
  );
  const txData = () => {
    if (operation === 'ETH') return { to, value: toHex(parseEther(amount || '0')), data: '0x' };
    const value = BigInt(amount || '0');
    let name: string, inputs: any[], args: any[];
    if (
      operation === 'ERC20 transfer' ||
      operation === 'ERC20 approve' ||
      operation === 'ERC20 revoke'
    ) {
      name = operation === 'ERC20 transfer' ? 'transfer' : 'approve';
      inputs = [{ type: 'address' }, { type: 'uint256' }];
      args = [to, operation === 'ERC20 revoke' ? 0n : value];
    } else if (operation === 'ERC721') {
      name = 'safeTransferFrom';
      inputs = [{ type: 'address' }, { type: 'address' }, { type: 'uint256' }];
      args = [addr, to, BigInt(token)];
    } else {
      name = 'safeTransferFrom';
      inputs = [
        { type: 'address' },
        { type: 'address' },
        { type: 'uint256' },
        { type: 'uint256' },
        { type: 'bytes' },
      ];
      args = [addr, to, BigInt(token), value, '0x'];
    }
    return {
      to: contract,
      value: '0x0',
      data: encodeFunctionData({
        abi: [{ type: 'function', name, stateMutability: 'nonpayable', inputs, outputs: [] }],
        functionName: name,
        args,
      }),
    };
  };
  return (
    <div className="shell">
      <aside>
        <div className="brand">
          <span className="mark">F</span>
          <div>
            FLIPPER<span>EVM WALLET</span>
          </div>
        </div>
        <nav>
          {(['home', 'send', 'assets', 'activity', 'sites', 'settings'] as const).map((n, i) => (
            <button className={tab === n ? 'active' : ''} key={n} onClick={() => setTab(n)}>
              <span className="nav-num">0{i + 1}</span>
              {t[n]}
            </button>
          ))}
        </nav>
        <div className="aside-note">
          <span className="dot" />
          {t.test}
          <small>{t.development}</small>
        </div>
      </aside>
      <main>
        <header>
          <div>
            <p className="eyebrow">FLIPPER ZERO · MOMENTUM</p>
            <h1>{t.title}</h1>
            <p className="muted">{t.sub}</p>
          </div>
          <label className="language-picker">
            <span>{t.language}</span>
            <select
              id="wallet-language"
              value={locale}
              disabled={busy}
              onChange={(e) => void act(() => call('locale', { value: e.target.value }))}
            >
              {languages.map(({ code, name }) => (
                <option key={code} value={code} lang={code}>
                  {name}
                </option>
              ))}
            </select>
          </label>
        </header>
        <section className="connection">
          <div>
            <span className={'status-dot ' + (snapshot.connected ? 'on' : '')} />
            {snapshot.connected ? t.connected : t.offline}
          </div>
          <button
            disabled={busy}
            onClick={() =>
              void act(async () => {
                await navigator.hid.requestDevice({ filters: [FILTER] });
                await call('connect');
              })
            }
          >
            {t.connect} ↗
          </button>
        </section>
        <div className="selectors">
          <label>
            {t.network}
            <select
              value={st.chain}
              disabled={busy || !!snapshot.pending}
              onChange={(e) => void act(() => call('chain', { value: Number(e.target.value) }))}
            >
              {Object.entries(CHAINS).map(([id, c]) => (
                <option key={id} value={id}>
                  {c.name}
                </option>
              ))}
            </select>
          </label>
          <label>
            {t.account}
            <select
              value={st.account}
              disabled={busy || !!snapshot.pending}
              onChange={(e) => void act(() => call('account', { value: Number(e.target.value) }))}
            >
              {[0, 1, 2, 3, 4].map((i) => (
                <option key={i} value={i}>
                  {t.account} {i + 1}
                </option>
              ))}
            </select>
          </label>
        </div>
        {error && (
          <div className="notice error" role="alert">
            {error}
          </div>
        )}
        {message && (
          <div className="notice" role="status">
            {message}
          </div>
        )}
        {snapshot.pending && (
          <section className="approval">
            <p className="eyebrow">{t.needApproval}</p>
            <h2>{t[snapshot.pending.title as RequestTitle]}</h2>
            <label>{t.origin}</label>
            <strong className="origin">{snapshot.pending.origin}</strong>
            <pre>
              {snapshot.pending.title === 'connectSite'
                ? t.shareAccount.replace('{account}', String(st.account + 1))
                : snapshot.pending.details}
            </pre>
            <p>{t.wait}</p>
            <div className="actions">
              <button
                className="secondary"
                onClick={() => void act(() => call('reject', { id: snapshot.pending.id }))}
              >
                {t.reject}
              </button>
              <button
                disabled={snapshot.pending.phase === 'device'}
                onClick={() => void act(() => call('approve', { id: snapshot.pending.id }))}
              >
                {snapshot.pending.phase === 'device' ? t.wait : t.approve}
              </button>
            </div>
          </section>
        )}
        {tab === 'home' && (
          <>
            <section className="balance-card">
              <p className="eyebrow">{chain.name}</p>
              <h2>
                {balance === null ? '—' : formatEther(BigInt(balance))}
                <span>ETH</span>
              </h2>
              <small>{t.test}</small>
              <div className="actions">
                <button
                  disabled={!addr || busy}
                  className="secondary"
                  onClick={() => void act(async () => setBalance(await call('balance')))}
                >
                  {t.refresh}
                </button>
                <button onClick={() => setTab('send')}>{t.send} →</button>
              </div>
            </section>
            <section className="card">
              <h2>{t.receive}</h2>
              <p className="address">{addr || t.noAddress}</p>
              <div className="actions">
                <button disabled={busy} onClick={() => void act(() => call('address'))}>
                  {t.verify}
                </button>
                <button
                  className="secondary"
                  disabled={!addr}
                  onClick={() => void navigator.clipboard.writeText(addr)}
                >
                  {t.copy}
                </button>
              </div>
              <p className="muted">{t.explain}</p>
            </section>
          </>
        )}
        {tab === 'send' && (
          <section className="card">
            <h2>{t.send}</h2>
            <form
              onSubmit={(e) => {
                e.preventDefault();
                void act(async () => {
                  const result = await call('send', { tx: txData() });
                  setMessage(String(result));
                });
              }}
            >
              <label>
                {t.operation}
                <select value={operation} onChange={(e) => setOperation(e.target.value)}>
                  {[
                    'ETH',
                    'ERC20 transfer',
                    'ERC20 approve',
                    'ERC20 revoke',
                    'ERC721',
                    'ERC1155',
                  ].map((o) => (
                    <option key={o} value={o}>
                      {o === 'ERC20 transfer'
                        ? t.transfer
                        : o === 'ERC20 approve'
                          ? t.allowance
                          : o === 'ERC20 revoke'
                            ? t.cancelAllowance
                            : o}
                    </option>
                  ))}
                </select>
              </label>
              {operation !== 'ETH' && field(t.contract, contract, setContract, '0x…')}
              {field(t.recipient, to, setTo, '0x…')}
              {['ERC721', 'ERC1155'].includes(operation) && field(t.token, token, setToken, '0')}
              {!['ERC721', 'ERC20 revoke'].includes(operation) &&
                field(
                  t.amount + (operation === 'ETH' ? ' (ETH)' : ` (${t.rawUnits})`),
                  amount,
                  setAmount,
                  '0',
                )}
              {operation !== 'ETH' && <p className="muted">{t.raw}</p>}
              <button type="submit" disabled={!addr || busy || !!snapshot.pending}>
                {t.review} →
              </button>
            </form>
          </section>
        )}
        {tab === 'assets' && (
          <section className="card">
            <h2>{t.assets}</h2>
            <p className="muted">{t.raw}</p>
            <form
              onSubmit={(e) => {
                e.preventDefault();
                void act(() =>
                  call('watch', { asset: { type: watchType, address: watchAddress, id: watchId } }),
                );
              }}
            >
              <label>
                {t.type}
                <select value={watchType} onChange={(e) => setWatchType(e.target.value)}>
                  {['ERC20', 'ERC721', 'ERC1155'].map((v) => (
                    <option key={v}>{v}</option>
                  ))}
                </select>
              </label>
              {field(t.contract, watchAddress, setWatchAddress, '0x…')}
              {watchType !== 'ERC20' && field(t.token, watchId, setWatchId, '0')}
              <button disabled={busy}>{t.watch}</button>
            </form>
            {st.assets.map((a: any, i: number) => (
              <article className="row" key={i}>
                <div>
                  <strong>
                    {a.type} · {CHAINS[a.chain as keyof typeof CHAINS]?.name}
                  </strong>
                  <p className="address">
                    {a.address}
                    {a.id !== undefined ? ' #' + a.id : ''}
                  </p>
                </div>
                <div className="actions">
                  <button
                    className="secondary"
                    onClick={() =>
                      void act(async () =>
                        setMessage(String(await call('assetBalance', { index: i }))),
                      )
                    }
                  >
                    {t.balance}
                  </button>
                  <button
                    className="ghost"
                    onClick={() => void act(() => call('unwatch', { index: i }))}
                  >
                    {t.remove}
                  </button>
                </div>
              </article>
            ))}
          </section>
        )}
        {tab === 'activity' && (
          <section className="card">
            <h2>{t.activity}</h2>
            {!st.history.length && <p className="muted">{t.empty}</p>}
            {st.history.map((h: any) => (
              <article className="row" key={h.hash}>
                <strong>
                  {CHAINS[h.chain as keyof typeof CHAINS].name} ·{' '}
                  {new Date(h.at).toLocaleString(locale)}
                </strong>
                <a
                  className="address"
                  href={CHAINS[h.chain as keyof typeof CHAINS].explorer + '/tx/' + h.hash}
                  target="_blank"
                  rel="noreferrer"
                >
                  {h.hash} ↗
                </a>
                <small>{h.origin}</small>
                <button
                  className="secondary"
                  onClick={() =>
                    void act(async () => {
                      const r = await call('receipt', { hash: h.hash });
                      setMessage(
                        r ? JSON.stringify({ status: r.status, block: r.blockNumber }) : t.pending,
                      );
                    })
                  }
                >
                  {t.receipt}
                </button>
              </article>
            ))}
          </section>
        )}
        {tab === 'sites' && (
          <section className="card">
            <h2>{t.sites}</h2>
            <p className="muted">{t.clear}</p>
            {!Object.keys(st.grants).length && <p>{t.empty}</p>}
            {Object.entries(st.grants).map(([origin, g]: [string, any]) => (
              <article className="row" key={origin}>
                <strong className="origin">{origin}</strong>
                <p className="address">{g.address}</p>
                <button
                  className="secondary"
                  onClick={() => void act(() => call('revoke', { origin }))}
                >
                  {t.revoke}
                </button>
              </article>
            ))}
          </section>
        )}
        {tab === 'settings' && (
          <>
            <section className="card">
              <h2>{t.backup}</h2>
              <p>{t.backupText}</p>
              <p className="muted">{t.security}</p>
            </section>
            <section className="card">
              <h2>{t.privacyTitle}</h2>
              <p>{t.privacy}</p>
              <p className="address">{chain.rpc}</p>
              <button className="secondary" onClick={() => void act(() => call('disconnect'))}>
                {t.disconnect}
              </button>
            </section>
          </>
        )}
        <footer>
          <span>{t.test}</span>
          <span>v0.1.0-dev · MIT</span>
        </footer>
      </main>
    </div>
  );
}
createRoot(document.getElementById('root')!).render(<App />);
