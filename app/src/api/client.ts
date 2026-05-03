import axios from 'axios';
import { DeviceStorage } from '../utils/storage';

/**
 * Dynamically creates an axios instance based on the currently stored device IP.
 */
export const getApiClient = async () => {
  const ip = await DeviceStorage.getIP();
  if (!ip) {
    throw new Error('Device not connected. Please scan QR code first.');
  }

  // Ensure protocol is present
  const baseURL = ip.startsWith('http') ? ip : `http://${ip}`;

  return axios.create({
    baseURL,
    timeout: 10000, // 10s timeout for discovery/settings
  });
};

/**
 * Specialized client for large file uploads with longer timeout.
 */
export const getTransferClient = async () => {
  const ip = await DeviceStorage.getIP();
  if (!ip) {
    throw new Error('Device not connected.');
  }

  const baseURL = ip.startsWith('http') ? ip : `http://${ip}`;

  return axios.create({
    baseURL,
    timeout: 300000, // 5 minutes for large books
    headers: {
      'Content-Type': 'multipart/form-data',
    },
  });
};
