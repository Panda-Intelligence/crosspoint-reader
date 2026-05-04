import { getApiClient } from './client';

/**
 * /api/status — small device-info endpoint exposed by the firmware
 * (handler at src/network/CrossPointWebServer.cpp:handleStatus).
 */
export interface DeviceStatus {
  /** Firmware build version string, e.g. "v6.18". */
  version: string;
  /** Current local IP, in either AP or STA mode. */
  ip: string;
  /** "AP" or "STA". */
  mode: string;
  /** Wi-Fi RSSI in dBm; 0 in AP mode. */
  rssi: number;
  /** Free heap in bytes. */
  freeHeap: number;
  /** Uptime in seconds since last boot. */
  uptime: number;
}

export const StatusApi = {
  async get(): Promise<DeviceStatus> {
    const client = await getApiClient();
    const response = await client.get<DeviceStatus>('/api/status');
    return response.data;
  },
};
