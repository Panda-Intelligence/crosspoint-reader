import { getTransferClient } from './client';

export interface UploadFile {
  uri: string;
  name: string;
  type: string;
}

export interface UploadOptions {
  /**
   * Absolute target directory on the device, e.g. "/library/scifi". When
   * omitted, the firmware drops the file at the SD card root ("/"). The
   * directory must already exist — create it via FilesApi.mkdir first.
   */
  targetPath?: string;
  onProgress?: (percent: number) => void;
}

// Legacy callback signature retained for older call sites.
type OptionsArg = UploadOptions | ((percent: number) => void) | undefined;

function normalize(opts: OptionsArg): UploadOptions {
  if (!opts) return {};
  if (typeof opts === 'function') return { onProgress: opts };
  return opts;
}

export const TransferApi = {
  /**
   * Uploads a file (EPUB, TXT, CBZ, or image) to the device.
   *
   * Pass `{ targetPath }` to direct the upload into a specific folder.
   * Firmware reads the path from the URL query string at upload START
   * (multipart fields aren't available until after the body is parsed),
   * see `src/network/CrossPointWebServer.cpp:741-751`.
   */
  async uploadFile(file: UploadFile, options?: OptionsArg): Promise<void> {
    const { targetPath, onProgress } = normalize(options);
    const client = await getTransferClient();

    const formData = new FormData();
    formData.append('file', {
      uri: file.uri,
      name: file.name,
      type: file.type,
    } as any);

    const url = targetPath
      ? `/upload?${new URLSearchParams({ path: targetPath }).toString()}`
      : '/upload';

    await client.post(url, formData, {
      onUploadProgress: (progressEvent) => {
        if (onProgress && progressEvent.total) {
          const percent = Math.round((progressEvent.loaded * 100) / progressEvent.total);
          onProgress(percent);
        }
      },
    });
  },
};
