import { getApiClient } from './client';
import { DeviceStorage } from '../utils/storage';

/**
 * /api/books/recent — recent books with cover paths.
 * Backed by RecentBooksStore on the device (cap 10, most-recent first).
 */
export interface RecentBook {
  path: string;       // EPUB / TXT file path on the SD card
  title: string;
  author: string;
  coverPath: string;  // BMP cover path on the SD card; "" if none
}

export const BooksApi = {
  async recent(): Promise<RecentBook[]> {
    const client = await getApiClient();
    const response = await client.get<RecentBook[]>('/api/books/recent');
    if (!Array.isArray(response.data)) {
      throw new Error('Unexpected /api/books/recent response (expected array)');
    }
    return response.data;
  },

  /**
   * Build a fully-qualified URL to the cover image for use as an
   * `<Image source={{ uri }}/>`. Returns null when the device hasn't
   * cached a cover for this book yet.
   *
   * Note: BMP loading is supported on iOS but unreliable on Android.
   * Callers should provide a placeholder for the load-error case.
   */
  async coverImageUrl(coverPath: string): Promise<string | null> {
    if (!coverPath) return null;
    const ip = await DeviceStorage.getIP();
    if (!ip) return null;
    const baseUrl = ip.startsWith('http') ? ip : `http://${ip}`;
    const token = await DeviceStorage.getToken();
    const qs = new URLSearchParams({ path: coverPath });
    if (token) qs.set('t', token);
    return `${baseUrl}/download?${qs.toString()}`;
  },
};
