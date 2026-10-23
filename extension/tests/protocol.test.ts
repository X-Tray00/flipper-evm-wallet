import { describe, it, expect } from 'vitest';
import { frames, Assembler, envelope } from '../src/hid';
import { chainId, quantity, webOrigin, accountIndex } from '../src/policy';
describe('HID protocol', () => {
  it('roundtrips a fragmented response and rejects replay', () => {
    const data = Uint8Array.from({ length: 66 }, (_, i) => i);
    const f = frames(0x83, 17, 23, data);
    const a = new Assembler(3, 17, 23);
    expect(a.accept(new DataView(f[0].buffer))).toBeUndefined();
    expect(a.accept(new DataView(f[1].buffer))).toEqual(data);
    expect(() => a.accept(new DataView(f[1].buffer))).toThrow();
  });
  it.each(['session', 'request', 'sequence', 'padding', 'size'])('rejects changed %s', (kind) => {
    const a = new Assembler(3, 17, 23),
      f = frames(0x83, 17, 23, new Uint8Array([0]))[0];
    if (kind === 'session') f[4]++;
    if (kind === 'request') f[8]++;
    if (kind === 'sequence') f[14]++;
    if (kind === 'padding') f[63] = 1;
    if (kind === 'size') f[12] = 255;
    expect(() => a.accept(new DataView(f.buffer))).toThrow();
  });
  it('bounds messages and origin', () => {
    expect(() => frames(3, 1, 1, new Uint8Array(8193))).toThrow();
    expect(() => envelope(0, 'x'.repeat(128))).toThrow();
    expect(frames(3, 1, 1, new Uint8Array(8192))).toHaveLength(187);
  });
});
describe('policy boundaries', () => {
  it('permits only three test networks', () => {
    for (const n of [11155111, 84532, 421614]) expect(chainId(n)).toBe(n);
    for (const n of [1, 10, 42161, 8453, 0, -1, 'evil']) expect(() => chainId(n)).toThrow();
  });
  it('rejects noncanonical and oversized integers', () => {
    for (const q of ['0x00', '0x01', '-1', '1', '0X1']) expect(() => quantity(q)).toThrow();
    expect(quantity('0x0')).toBe(0n);
    expect(() => quantity('0x100', 255n)).toThrow();
  });
  it('derives origin from browser URL and forbids opaque origins', () => {
    expect(webOrigin('https://example.com/path')).toBe('https://example.com');
    expect(() => webOrigin('data:text/plain,test')).toThrow();
    expect(() => webOrigin('http://example.com')).toThrow();
    expect(() => accountIndex(5)).toThrow();
  });
});

describe('cancelled device replies', () => {
  it('does not let a late signing/cancel response poison the next request', async () => {
    const { WalletHid } = await import('../src/hid');
    const { vi } = await import('vitest');
    class Device extends EventTarget {
      opened = true;
      vendorId = 0x0483;
      productId = 0x5741;
      collections = [{ usagePage: 0xff71 }];
      async open() {}
      async close() {
        this.opened = false;
      }
      emit(command: number, id: number, payload: Uint8Array) {
        for (const f of frames(command | 128, 17, id, payload)) {
          const e = Object.assign(new Event('inputreport'), {
            data: new DataView(f.buffer),
            reportId: 0,
          });
          this.dispatchEvent(e);
        }
      }
      async sendReport(_: number, b: Uint8Array) {
        if (b[3] === 1)
          queueMicrotask(() => this.emit(1, 1, new Uint8Array([0, 1, 0, 0, 0, 0, 0, 0])));
      }
    }
    const device = new Device();
    const service = Object.assign(new EventTarget(), { getDevices: async () => [device] });
    vi.stubGlobal('navigator', { hid: service });
    const hid = new WalletHid();
    try {
      await hid.connect();
      const cancelled = hid.request(3, new Uint8Array([0])).catch((e) => e.code);
      await hid.cancel();
      expect(await cancelled).toBe(4001);
      const next = hid.request(7, new Uint8Array([0]));
      device.emit(3, 1, new Uint8Array([5]));
      device.emit(6, 2, new Uint8Array([5]));
      device.emit(7, 3, new Uint8Array([0, 42]));
      expect(await next).toEqual(new Uint8Array([42]));
    } finally {
      await hid.close();
      vi.unstubAllGlobals();
    }
  });
});
