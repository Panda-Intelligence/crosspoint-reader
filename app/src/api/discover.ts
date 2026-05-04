import * as Network from 'expo-network';

export interface DiscoveredDevice {
  /** Full base URL, e.g. "http://192.168.1.50" */
  ip: string;
  /** Round-trip time of the probe in ms (rough connectivity hint). */
  reachableMs: number;
  /** True if the device responded 401 — alive but token required. */
  needsAuth: boolean;
}

/**
 * Crosspoint Reader's native UDP discovery beacon (port 8134, "hello" → reply)
 * is the canonical mechanism, but raw UDP requires a native module not
 * available in Expo Go / managed workflow. As a portable fallback this scans
 * the local /24 subnet over HTTP, probing /api/settings on each candidate IP.
 *
 * 200 OR 401 is treated as "Crosspoint server alive". 401 means a paired
 * token is required — caller should suggest QR pairing for full access.
 */

const PROBE_TIMEOUT_MS = 1500;
const CONCURRENCY = 32;

async function probe(ip: string): Promise<DiscoveredDevice | null> {
  const baseUrl = `http://${ip}`;
  const start = Date.now();
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), PROBE_TIMEOUT_MS);
  try {
    const res = await fetch(`${baseUrl}/api/settings`, {
      method: 'GET',
      signal: controller.signal,
    });
    if (res.status === 200 || res.status === 401) {
      return {
        ip: baseUrl,
        reachableMs: Date.now() - start,
        needsAuth: res.status === 401,
      };
    }
    return null;
  } catch {
    return null;
  } finally {
    clearTimeout(timer);
  }
}

export interface ScanProgress {
  probed: number;
  total: number;
}

export async function discoverDevices(
  onProgress?: (p: ScanProgress) => void,
): Promise<DiscoveredDevice[]> {
  const localIp = await Network.getIpAddressAsync();
  if (!localIp) {
    throw new Error('Could not determine local IP. Connect to Wi-Fi first.');
  }
  const match = localIp.match(/^(\d+)\.(\d+)\.(\d+)\.(\d+)$/);
  if (!match) {
    throw new Error(`Unsupported IP format: ${localIp}`);
  }
  const [, a, b, c, dStr] = match;
  const ownLast = parseInt(dStr, 10);

  const candidates: string[] = [];
  for (let i = 1; i <= 254; i++) {
    if (i === ownLast) continue;
    candidates.push(`${a}.${b}.${c}.${i}`);
  }

  const found: DiscoveredDevice[] = [];
  let probed = 0;
  const total = candidates.length;
  const queue = [...candidates];

  const worker = async () => {
    while (queue.length > 0) {
      const next = queue.shift();
      if (!next) return;
      const result = await probe(next);
      if (result) found.push(result);
      probed++;
      onProgress?.({ probed, total });
    }
  };

  const workers: Promise<void>[] = [];
  for (let w = 0; w < CONCURRENCY; w++) {
    workers.push(worker());
  }
  await Promise.all(workers);

  return found;
}
