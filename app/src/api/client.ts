import axios, { AxiosInstance, isAxiosError } from 'axios';

import { DeviceStorage } from '../utils/storage';

/**
 * Global hook for any axios call that returns 401 (Unauthorized).
 *
 * Screens can register a handler in their effect chain (typically the
 * root layout) that routes the user back to the QR pairing flow. We
 * keep this as a single-subscriber callback rather than an event bus
 * because there's only ever one router in the app.
 */
let unauthorizedHandler: (() => void) | null = null;

export function setUnauthorizedHandler(handler: (() => void) | null): void {
  unauthorizedHandler = handler;
}

/**
 * Determines whether an axios error is a 401 from the device. We
 * deliberately limit to authoritative responses (not transport errors)
 * so a flaky Wi-Fi blip doesn't punt the user back to the scanner.
 */
export function isUnauthorizedError(err: unknown): boolean {
  return isAxiosError(err) && err.response?.status === 401;
}

function attachInterceptors(inst: AxiosInstance): AxiosInstance {
  inst.interceptors.response.use(
    (r) => r,
    (err) => {
      if (isUnauthorizedError(err) && unauthorizedHandler) {
        try {
          unauthorizedHandler();
        } catch {
          // Never let the handler throw — the original error must
          // still propagate to the caller.
        }
      }
      return Promise.reject(err);
    },
  );
  return inst;
}

export const getApiClient = async (): Promise<AxiosInstance> => {
  const ip = await DeviceStorage.getIP();
  if (!ip) {
    throw new Error('Device not connected. Please scan QR code first.');
  }

  const token = await DeviceStorage.getToken();
  const baseURL = ip.startsWith('http') ? ip : `http://${ip}`;

  const headers: Record<string, string> = {};
  if (token) {
    headers.Authorization = `Bearer ${token}`;
  }

  return attachInterceptors(
    axios.create({
      baseURL,
      timeout: 10000,
      headers,
    }),
  );
};

/**
 * Specialized client for large file uploads with longer timeout.
 */
export const getTransferClient = async (): Promise<AxiosInstance> => {
  const ip = await DeviceStorage.getIP();
  if (!ip) {
    throw new Error('Device not connected.');
  }

  const token = await DeviceStorage.getToken();
  const baseURL = ip.startsWith('http') ? ip : `http://${ip}`;

  const headers: Record<string, string> = {
    'Content-Type': 'multipart/form-data',
  };
  if (token) {
    headers.Authorization = `Bearer ${token}`;
  }

  return attachInterceptors(
    axios.create({
      baseURL,
      timeout: 300000,
      headers,
    }),
  );
};
