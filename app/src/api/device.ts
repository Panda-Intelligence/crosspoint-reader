import { getApiClient } from './client';

export interface DeviceSettings {
  language?: number;
  orientation?: number;
  fontFamily?: number;
  fontSize?: number;
  lineSpacing?: number;
  screenMargin?: number;
  [key: string]: any;
}

export const DeviceApi = {
  /**
   * Check if the device is reachable and responsive.
   */
  async ping(): Promise<boolean> {
    try {
      const client = await getApiClient();
      // Usually, hitting the root or /api/settings is enough to check connectivity
      const response = await client.get('/');
      return response.status === 200;
    } catch (error) {
      return false;
    }
  },

  /**
   * Fetch current settings from the device.
   */
  async getSettings(): Promise<DeviceSettings> {
    const client = await getApiClient();
    const response = await client.get('/api/settings');
    return response.data;
  },

  /**
   * Update settings on the device.
   */
  async updateSettings(settings: DeviceSettings): Promise<void> {
    const client = await getApiClient();
    await client.post('/api/settings', settings);
  },
};
