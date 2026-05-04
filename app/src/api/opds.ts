import { getApiClient } from './client';

/**
 * /api/opds protocol (firmware: src/network/CrossPointWebServer.cpp:1432-1544).
 *
 * GET  /api/opds          →  OpdsServer[]
 * POST /api/opds          ←  OpdsServerPayload  (add if no index, else update)
 * POST /api/opds/delete   ←  { index: number }
 *
 * Passwords are NEVER returned by the device — only `hasPassword`. When
 * editing, OMIT the `password` field to preserve the existing password;
 * include an empty string to explicitly clear it.
 */

export interface OpdsServer {
  index: number;
  name: string;
  url: string;
  username: string;
  hasPassword: boolean;
}

export interface OpdsServerPayload {
  /** When present, update existing entry at this index. Omit to add a new server. */
  index?: number;
  name: string;
  url: string;
  username: string;
  /** Omit to preserve existing password; empty string to clear. */
  password?: string;
}

export const OpdsApi = {
  async list(): Promise<OpdsServer[]> {
    const client = await getApiClient();
    const response = await client.get<OpdsServer[]>('/api/opds');
    if (!Array.isArray(response.data)) {
      throw new Error('Unexpected /api/opds response (expected array)');
    }
    return response.data;
  },

  async save(payload: OpdsServerPayload): Promise<void> {
    const client = await getApiClient();
    await client.post('/api/opds', payload);
  },

  async remove(index: number): Promise<void> {
    const client = await getApiClient();
    await client.post('/api/opds/delete', { index });
  },
};
