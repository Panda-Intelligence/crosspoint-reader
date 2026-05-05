import { getApiClient } from './client';

/**
 * Settings API protocol (matches firmware src/network/CrossPointWebServer.cpp).
 *
 * GET  /api/settings  →  DeviceSetting[]   (metadata + current value)
 * POST /api/settings  ←  Record<key,value>  (flat diff; unsent keys untouched)
 *
 * Server-side type strings: "toggle" | "enum" | "value" | "string"
 */

export type SettingValue = number | string;

interface SettingBase {
  key: string;
  name: string;     // already i18n-resolved by firmware
  category: string; // already i18n-resolved by firmware
  value: SettingValue;
}

export interface ToggleSetting extends SettingBase {
  type: 'toggle';
  value: number; // 0 | 1
}

export interface EnumSetting extends SettingBase {
  type: 'enum';
  value: number;
  options: string[];
}

export interface ValueSetting extends SettingBase {
  type: 'value';
  value: number;
  min: number;
  max: number;
  step: number;
}

export interface StringSetting extends SettingBase {
  type: 'string';
  value: string;
}

export type DeviceSetting =
  | ToggleSetting
  | EnumSetting
  | ValueSetting
  | StringSetting;

export const DeviceApi = {
  /**
   * Probe the device. Returns true if the server responded — including 401
   * (auth required), which still proves the device is alive on the network.
   */
  async ping(): Promise<boolean> {
    try {
      const client = await getApiClient();
      const response = await client.get('/api/settings', { timeout: 4000 });
      return response.status === 200;
    } catch (error: any) {
      // axios throws on non-2xx; 401 means "reachable but unauthorized"
      if (error?.response?.status === 401) {
        return true;
      }
      return false;
    }
  },

  /**
   * Fetch the full settings array (metadata + current values).
   */
  async getSettings(): Promise<DeviceSetting[]> {
    const client = await getApiClient();
    const response = await client.get<DeviceSetting[]>('/api/settings');
    if (!Array.isArray(response.data)) {
      throw new Error('Unexpected /api/settings response (expected array)');
    }
    return response.data;
  },

  /**
   * Push a partial diff. Send only keys whose value has changed; the firmware
   * applies the subset and silently ignores out-of-range / unknown keys.
   */
  async updateSettings(diff: Record<string, SettingValue>): Promise<void> {
    const client = await getApiClient();
    await client.post('/api/settings', diff);
  },

  /**
   * Reset the device's user-facing preferences (Display / Reader / Controls
   * / System enums + toggles + values + display strings) back to factory
   * defaults. The firmware deliberately preserves the API token, the
   * lock-screen passcode + lockout state, and legacy OPDS credentials —
   * see CrossPointSettings::resetUserPreferencesToDefaults.
   */
  async resetSettings(): Promise<void> {
    const client = await getApiClient();
    await client.post('/api/settings/reset');
  },
};
