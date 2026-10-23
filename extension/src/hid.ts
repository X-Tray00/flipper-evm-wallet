import { WalletError, fail } from './policy';
export const FILTER = { vendorId: 0x0483, productId: 0x5741, usagePage: 0xff71, usage: 1 };
export const CHUNK = 44;
export function frames(command: number, session: number, request: number, payload: Uint8Array) {
  if (payload.length > 8192) fail(-32602, 'Device request exceeds 8 KiB');
  const result: Uint8Array[] = [];
  for (let seq = 0; seq < Math.max(1, Math.ceil(payload.length / CHUNK)); seq++) {
    const f = new Uint8Array(64),
      v = new DataView(f.buffer);
    f.set([70, 87, 1, command]);
    v.setUint32(4, session, true);
    v.setUint32(8, request, true);
    v.setUint16(12, payload.length, true);
    v.setUint16(14, seq, true);
    f.set(payload.slice(seq * CHUNK, (seq + 1) * CHUNK), 20);
    result.push(f);
  }
  return result;
}
export class Assembler {
  private bytes = new Uint8Array(128);
  private total = 0;
  private received = 0;
  private sequence = 0;
  session = 0;
  constructor(
    private command: number,
    private expectedSession: number,
    private request: number,
  ) {}
  accept(raw: DataView): Uint8Array | undefined {
    if (raw.byteLength !== 64) fail(-32603, 'Invalid HID report size');
    const f = new Uint8Array(raw.buffer, raw.byteOffset, 64);
    const session = raw.getUint32(4, true),
      request = raw.getUint32(8, true),
      total = raw.getUint16(12, true),
      seq = raw.getUint16(14, true);
    if (
      f[0] !== 70 ||
      f[1] !== 87 ||
      f[2] !== 1 ||
      f[3] !== (this.command | 128) ||
      request !== this.request ||
      !session ||
      (this.command !== 1 && session !== this.expectedSession) ||
      seq !== this.sequence ||
      total < 1 ||
      total > 128 ||
      f.slice(16, 20).some(Boolean)
    )
      fail(-32603, 'Invalid HID response sequence');
    if (seq === 0) {
      this.total = total;
      this.session = session;
    } else if (total !== this.total || session !== this.session)
      fail(-32603, 'HID response changed');
    const take = Math.min(CHUNK, total - this.received);
    if (take < 1 || f.slice(20 + take).some(Boolean)) fail(-32603, 'Invalid HID padding');
    this.bytes.set(f.subarray(20, 20 + take), this.received);
    this.received += take;
    this.sequence++;
    if (this.received === total) return this.bytes.slice(0, total);
    return undefined;
  }
}
const deviceErrors = [
  '',
  'Malformed request',
  'Unsupported action or network',
  'Unlock wallet on Flipper',
  'Device is busy',
  'Cancelled on device',
  'Device review timed out',
  'USB sequence changed',
  'Cryptography failed',
  'Storage failed',
];
export class WalletHid {
  private device?: HIDDevice;
  private session = 0;
  private next = 1;
  private cancelled = new Set<string>();
  private pending?: {
    assembler: Assembler;
    key: string;
    resolve: (b: Uint8Array) => void;
    reject: (e: Error) => void;
    timer: ReturnType<typeof setTimeout>;
  };
  get connected() {
    return !!this.device?.opened && !!this.session;
  }
  async connect() {
    if (this.connected) return;
    const all = (await navigator.hid.getDevices()).filter(
      (d) =>
        d.vendorId === FILTER.vendorId &&
        d.productId === FILTER.productId &&
        d.collections.some((c) => c.usagePage === FILTER.usagePage),
    );
    if (all.length !== 1) fail(4900, 'Use Connect Flipper and select exactly one wallet device');
    this.device = all[0];
    if (!this.device.opened) await this.device.open();
    this.device.addEventListener('inputreport', this.receive);
    navigator.hid.addEventListener('disconnect', this.disconnected);
    const hello = await this.request(1, new Uint8Array());
    if (hello.length !== 7 || hello[0] !== 1) {
      await this.close();
      fail(4900, 'Unsupported device protocol');
    }
  }
  private disconnected = (e: HIDConnectionEvent) => {
    if (e.device === this.device) {
      this.abort(new WalletError(4900, 'Flipper disconnected'));
      this.session = 0;
    }
  };
  private receive = (event: HIDInputReportEvent) => {
    if (
      event.data.byteLength === 64 &&
      this.cancelled.has(
        `${event.data.getUint32(4, true)}:${event.data.getUint32(8, true)}:${event.data.getUint8(3)}`,
      )
    )
      return;
    const p = this.pending;
    if (!p) return;
    try {
      if (event.reportId !== 0) fail(-32603, 'Unexpected HID report ID');
      const b = p.assembler.accept(event.data);
      if (!b) return;
      this.session = p.assembler.session;
      clearTimeout(p.timer);
      this.pending = undefined;
      if (b[0])
        p.reject(new WalletError(b[0] === 5 ? 4001 : -32603, deviceErrors[b[0]] || 'Device error'));
      else p.resolve(b.slice(1));
    } catch (e) {
      this.abort(e instanceof Error ? e : new Error('Invalid device response'));
      this.session = 0;
    }
  };
  private abort(error: Error) {
    if (this.pending) {
      clearTimeout(this.pending.timer);
      this.pending.reject(error);
      this.pending = undefined;
    }
  }
  async request(command: number, payload: Uint8Array): Promise<Uint8Array> {
    if (!this.device?.opened) fail(4900, 'Connect Flipper first');
    if (this.pending) fail(-32002, 'Another device request is active');
    if (command !== 1 && !this.session) fail(4900, 'Reconnect Flipper');
    const id = command === 1 ? 1 : this.next++;
    const session = command === 1 ? 0 : this.session;
    if (id >= 0xffffffff) fail(4900, 'Reconnect to start a new session');
    const promise = new Promise<Uint8Array>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.abort(new WalletError(4001, 'Device review timed out'));
        void this.cancel();
      }, 185000);
      this.pending = {
        assembler: new Assembler(command, session, id),
        key: `${session}:${id}:${command | 128}`,
        resolve,
        reject,
        timer,
      };
    });
    try {
      for (const frame of frames(command, session, id, payload))
        await this.device.sendReport(0, frame as Uint8Array<ArrayBuffer>);
    } catch (e) {
      this.abort(new WalletError(4900, e instanceof Error ? e.message : 'USB write failed'));
    }
    return promise;
  }
  async cancel() {
    if (this.pending) this.cancelled.add(this.pending.key);
    this.abort(new WalletError(4001, 'Request cancelled'));
    if (this.device?.opened && this.session) {
      const id = this.next++;
      this.cancelled.add(`${this.session}:${id}:134`);
      while (this.cancelled.size > 64) this.cancelled.delete(this.cancelled.values().next().value!);
      for (const f of frames(6, this.session, id, new Uint8Array([0])))
        await this.device.sendReport(0, f as Uint8Array<ArrayBuffer>).catch(() => {});
    }
  }
  async close() {
    await this.cancel();
    if (this.device) {
      this.device.removeEventListener('inputreport', this.receive);
      await this.device.close().catch(() => {});
    }
    navigator.hid.removeEventListener('disconnect', this.disconnected);
    this.device = undefined;
    this.session = 0;
    this.next = 1;
    this.cancelled.clear();
  }
}
export function envelope(account: number, origin: string, body: Uint8Array = new Uint8Array()) {
  const o = new TextEncoder().encode(origin);
  if (o.length < 1 || o.length > 127) fail(-32602, 'Origin length unsupported');
  const b = new Uint8Array(2 + o.length + body.length);
  b.set([account, o.length]);
  b.set(o, 2);
  b.set(body, 2 + o.length);
  return b;
}
