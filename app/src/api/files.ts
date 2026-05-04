import {
  downloadAsync,
  getInfoAsync,
  makeDirectoryAsync,
  Paths,
} from 'expo-file-system';

import { getApiClient } from './client';
import { DeviceStorage } from '../utils/storage';

/**
 * /api/files protocol (firmware: src/network/CrossPointWebServer.cpp).
 *
 * GET   /api/files?path=<dir>      → DeviceFileEntry[]  (directory listing)
 * GET   /download?path=<file>      → binary attachment
 * POST  /mkdir         (form)      → name + path
 * POST  /delete        (form)      → path  (or paths as JSON array)
 *
 * All mutation endpoints use application/x-www-form-urlencoded bodies,
 * NOT JSON. Pass `URLSearchParams` to axios so it picks the right
 * Content-Type automatically.
 */

export interface DeviceFileEntry {
  name: string;
  size: number;
  isDirectory: boolean;
  isEpub: boolean;
}

export interface DownloadResult {
  /** local file:// URI of the saved file */
  localUri: string;
  /** byte size on disk */
  size: number;
}

function joinPath(parent: string, child: string): string {
  if (!parent || parent === '/') return `/${child}`;
  if (parent.endsWith('/')) return `${parent}${child}`;
  return `${parent}/${child}`;
}

function parentOf(path: string): string {
  if (!path || path === '/') return '/';
  const trimmed = path.endsWith('/') ? path.slice(0, -1) : path;
  const idx = trimmed.lastIndexOf('/');
  if (idx <= 0) return '/';
  return trimmed.slice(0, idx);
}

export const FilesApi = {
  joinPath,
  parentOf,

  async list(path: string): Promise<DeviceFileEntry[]> {
    const client = await getApiClient();
    const response = await client.get<DeviceFileEntry[]>('/api/files', {
      params: { path },
    });
    if (!Array.isArray(response.data)) {
      throw new Error('Unexpected /api/files response (expected array)');
    }
    // Sort: directories first, then alphabetical.
    return [...response.data].sort((a, b) => {
      if (a.isDirectory !== b.isDirectory) return a.isDirectory ? -1 : 1;
      return a.name.localeCompare(b.name);
    });
  },

  /**
   * Downloads a remote file to a stable cache location under the app's
   * document directory: <doc>crosspoint/<basename>. Overwrites any prior
   * copy with the same name.
   */
  async download(remotePath: string): Promise<DownloadResult> {
    const ip = await DeviceStorage.getIP();
    if (!ip) throw new Error('Device not connected.');
    const baseUrl = ip.startsWith('http') ? ip : `http://${ip}`;
    const token = await DeviceStorage.getToken();
    const qs = new URLSearchParams({ path: remotePath });
    if (token) qs.set('t', token);
    const remoteUrl = `${baseUrl}/download?${qs.toString()}`;

    const basename = remotePath.split('/').filter(Boolean).pop() ?? 'download';
    const docUri = Paths.document.uri;
    const targetDir = `${docUri.endsWith('/') ? docUri : `${docUri}/`}crosspoint/`;
    const targetUri = `${targetDir}${basename}`;

    const dirInfo = await getInfoAsync(targetDir);
    if (!dirInfo.exists) {
      await makeDirectoryAsync(targetDir, { intermediates: true });
    }

    const result = await downloadAsync(remoteUrl, targetUri);
    if (result.status < 200 || result.status >= 300) {
      throw new Error(`Download failed (HTTP ${result.status})`);
    }
    const fileInfo = await getInfoAsync(result.uri);
    return {
      localUri: result.uri,
      size: fileInfo.exists ? (fileInfo.size ?? 0) : 0,
    };
  },

  async remove(path: string): Promise<void> {
    const client = await getApiClient();
    const body = new URLSearchParams({ path });
    await client.post('/delete', body.toString(), {
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    });
  },

  async mkdir(parent: string, name: string): Promise<void> {
    const client = await getApiClient();
    const body = new URLSearchParams({ path: parent, name });
    await client.post('/mkdir', body.toString(), {
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    });
  },
};
