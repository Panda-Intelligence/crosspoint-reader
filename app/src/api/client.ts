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

  const token = await DeviceStorage.getToken();
  const baseURL = ip.startsWith('http') ? ip : `http://${ip}`;

  const headers: any = {};
  if (token) {
    headers['Authorization'] = `Bearer ${token}`;
  }

  return axios.create({
    baseURL,
    timeout: 10000, // 10s timeout for discovery/settings
    headers,
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

  const token = await DeviceStorage.getToken();
  const baseURL = ip.startsWith('http') ? ip : `http://${ip}`;

  const headers: any = {
    'Content-Type': 'multipart/form-data',
  };
  if (token) {
    headers['Authorization'] = `Bearer ${token}`;
  }

  return axios.create({
    baseURL,
    timeout: 300000, // 5 minutes for large books
    headers,
  });
};
