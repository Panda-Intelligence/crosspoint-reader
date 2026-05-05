// Murphy Reader changelog data — update with each release
// 所有內容使用繁體中文

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
          "Web USB 刷機工具：基於 ESP Web Tools，支援 Chrome/Edge 瀏覽器",
          "OTA 更新 API（`/ota/latest`、`/ota/check`）",
          "Cloudflare Worker 託管 R2 韌體存儲與分發",
          "觸控偏移校準參數（`MOFEI_TOUCH_OFFSET_X/Y`）",
          "六項進階閱讀器功能：簡轉繁文字轉換、豎排 CJK 佈局、文本範圍高亮選擇、腳註正文彈出、全書搜尋、閱讀器內字典查詢",
          "Mate 伴侶 App：OAuth 環境變數配置、主題常數模組、無障礙標籤",
          "底部實體按鍵提示已隱藏（純觸控螢幕模式）",
          "Mofei QEMU 模擬器周邊（FT6336U 觸控、GDEQ0426T82 電子紙顯示）",
        ],
      },
      {
        category: "Changed",
        items: [
          "EPUB 章節快取版本升級至 23",
          "鍵盤輸入改為觸控點擊輸入字元",
          "ActivityResult 流程增強以支援閱讀器子活動",
        ],
      },
      {
        category: "Fixed",
        items: [
          "FilesApi 重複下載檔案前先刪除舊檔",
          "401 錯誤改為導向 QR 配對頁面，不再顯示通用錯誤提示",
          "`pio check` 靜態分析缺陷已修復",
        ],
      },
    ],
  },
];
