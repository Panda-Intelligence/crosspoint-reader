import * as SecureStore from 'expo-secure-store';

const DEVICE_IP_KEY = 'murphy_mate_device_ip';
const DEVICE_TOKEN_KEY = 'murphy_mate_device_token';

export const DeviceStorage = {
  async setIPAndToken(url: string) {
    try {
      const urlObj = new URL(url);
      const ip = urlObj.origin;
      const token = urlObj.searchParams.get('t');

      await SecureStore.setItemAsync(DEVICE_IP_KEY, ip);
      if (token) {
        await SecureStore.setItemAsync(DEVICE_TOKEN_KEY, token);
      } else {
        await SecureStore.deleteItemAsync(DEVICE_TOKEN_KEY);
      }
    } catch (e) {
      // Fallback if URL parsing fails but it's a simple string
      const sanitizedIp = url.trim().replace(/\/$/, '');
      await SecureStore.setItemAsync(DEVICE_IP_KEY, sanitizedIp);
      await SecureStore.deleteItemAsync(DEVICE_TOKEN_KEY);
    }
  },

  async getIP(): Promise<string | null> {
    return await SecureStore.getItemAsync(DEVICE_IP_KEY);
  },

  async getToken(): Promise<string | null> {
    return await SecureStore.getItemAsync(DEVICE_TOKEN_KEY);
  },

  async clearDevice() {
    await SecureStore.deleteItemAsync(DEVICE_IP_KEY);
    await SecureStore.deleteItemAsync(DEVICE_TOKEN_KEY);
  }
};
