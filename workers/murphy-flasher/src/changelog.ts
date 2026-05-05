// Murphy Reader changelog data — update with each release

export interface ChangelogEntry {
  version: string;
  date: string;
  changes: {
    category: "Added" | "Changed" | "Fixed" | "Removed";
    items: string[];
  }[];
}

export const changelog: ChangelogEntry[] = [
  {
    version: "1.0.0",
    date: "2026-05-05",
    changes: [
      {
        category: "Added",
        items: [
          "Web USB flasher via ESP Web Tools (Chrome/Edge)",
          "OTA update API (`/ota/latest`, `/ota/check`)",
          "Cloudflare Worker serving firmware from R2",
          "Touch offset calibration params (`MOFEI_TOUCH_OFFSET_X/Y`)",
          "Six advanced Reader features (簡轉繁, 竖排, 高亮, 脚注弹出, 全书搜索, 字典)",
          "Mate companion app: OAuth env config, theme constants, a11y labels",
          "Bottom button hints hidden (touch screen only)",
          "Mofei QEMU simulator peripherals (FT6336U, GDEQ0426T82)",
        ],
      },
      {
        category: "Changed",
        items: [
          "EPUB Section cache version bumped to 23",
          "Keyboard entry uses touch tap for character input",
          "ActivityResult flow enhanced for reader sub-activities",
        ],
      },
      {
        category: "Fixed",
        items: [
          "File re-download pre-delete in FilesApi",
          "401 errors redirect to QR pairing instead of generic toast",
          "`pio check` defects resolved",
        ],
      },
    ],
  },
];
