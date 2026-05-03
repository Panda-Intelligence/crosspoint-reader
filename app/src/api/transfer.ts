import { getTransferClient } from './client';

export interface UploadFile {
  uri: string;
  name: string;
  type: string;
}

export const TransferApi = {
  /**
   * Uploads a file (EPUB, TXT, CBZ, or Image) to the device.
   * @param file The file picked from the device.
   * @param onProgress Callback for tracking upload percentage.
   */
  async uploadFile(
    file: UploadFile,
    onProgress?: (percent: number) => void
  ): Promise<void> {
    const client = await getTransferClient();
    
    const formData = new FormData();
    // 'file' is the standard field name expected by most ESP32 WebServers
    formData.append('file', {
      uri: file.uri,
      name: file.name,
      type: file.type,
    } as any);

    await client.post('/upload', formData, {
      onUploadProgress: (progressEvent) => {
        if (onProgress && progressEvent.total) {
          const percent = Math.round((progressEvent.loaded * 100) / progressEvent.total);
          onProgress(percent);
        }
      },
    });
  },
};
