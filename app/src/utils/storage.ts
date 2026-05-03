import * as SecureStore from 'expo-secure-store';

const DEVICE_IP_KEY = 'murphy_mate_device_ip';

export const DeviceStorage = {
  async setIP(ip: string) {
    // Basic validation to ensure it's a URL or IP
    const sanitizedIp = ip.trim().replace(/\/$/, '');
    await SecureStore.setItemAsync(DEVICE_IP_KEY, sanitizedIp);
  },

  async getIP(): Promise<string | null> {
    return await SecureStore.getItemAsync(DEVICE_IP_KEY);
  },

  async clearIP() {
    await SecureStore.deleteItemAsync(DEVICE_IP_KEY);
  }
};
