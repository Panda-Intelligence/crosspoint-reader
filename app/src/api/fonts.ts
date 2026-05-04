import { getApiClient } from './client';

/**
 * /api/fonts protocol (firmware: src/network/CrossPointWebServer.cpp:483-549).
 *
 * GET  /api/fonts          →  { packs: FontPack[] }
 * POST /api/fonts/reload   →  200 text / 400 text
 * POST /api/fonts/delete   ←  { path: string }   →  200/404/500 text
 */

export interface FontPack {
  /** Display name shown in firmware-generated UI. */
  name: string;
  /** Filesystem path on the device, e.g. "/.mofei/fonts/notosans_tc_8.epf". */
  path: string;
  /** Whether the pack file is present on the SD card. */
  installed: boolean;
  /** Whether the pack is currently loaded into the renderer. */
  active: boolean;
}

interface FontPacksResponse {
  packs: FontPack[];
}

export const FontApi = {
  async list(): Promise<FontPack[]> {
    const client = await getApiClient();
    const response = await client.get<FontPacksResponse>('/api/fonts');
    return response.data?.packs ?? [];
  },

  /** Re-scan /.mofei/fonts/ and rebuild the active font mapping. */
  async reload(): Promise<void> {
    const client = await getApiClient();
    await client.post('/api/fonts/reload');
  },

  /** Delete a font pack file by its absolute path on the device. */
  async remove(path: string): Promise<void> {
    const client = await getApiClient();
    await client.post('/api/fonts/delete', { path });
  },
};
