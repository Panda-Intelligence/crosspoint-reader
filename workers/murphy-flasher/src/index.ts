// Murphy Flasher — Cloudflare Worker
// Web USB flasher (esp-web-tools) + OTA update API
// Routes: / | /changelog | /changelog.json | /manifest.json | /ota/latest | /ota/check | /firmware/*

import { changelog, type ChangelogEntry } from "./changelog";

interface Env {
  FIRMWARE_BUCKET: R2Bucket;
  FIRMWARE_META: KVNamespace;
}

const FIRMWARE_VERSION = "1.0.0";
const FIRMWARE_BUILD_DATE = "2026-05-05";
const FIRMWARE_FILENAME = "murphy-26-0505-1.0.0.bin";
const CHIP_FAMILY = "ESP32-S3";
const PRODUCT_NAME = "Murphy Reader";
const APP_OFFSET = 0x20000;

// HTML template with esp-web-tools integration
function serveIndex(): Response {
  const html = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>${PRODUCT_NAME} — Flash & Update</title>
  <style>
    :root {
      --esp-tools-button-color: #1a1a2e;
      --esp-tools-button-text-color: #e0e0e0;
      --esp-tools-button-border-radius: 8px;
    }
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: #0f0f1a;
      color: #e0e0e0;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 2rem 1rem;
    }
    .container {
      max-width: 640px;
      width: 100%;
      text-align: center;
    }
    .logo {
      font-size: 2rem;
      font-weight: 700;
      margin-bottom: 0.5rem;
      color: #7c8aff;
    }
    .subtitle {
      color: #8888aa;
      margin-bottom: 2rem;
    }
    .card {
      background: #1a1a2e;
      border-radius: 12px;
      padding: 2rem;
      margin-bottom: 1.5rem;
      border: 1px solid #2a2a4a;
    }
    .card h3 {
      margin-bottom: 0.75rem;
      color: #7c8aff;
    }
    .card p {
      color: #aaa;
      font-size: 0.9rem;
      line-height: 1.6;
    }
    .version {
      font-size: 0.85rem;
      color: #666;
      margin-top: 2rem;
    }
    .steps {
      text-align: left;
      padding-left: 1.5rem;
      color: #aaa;
      font-size: 0.9rem;
      line-height: 2;
    }
    .steps li { margin-bottom: 0.25rem; }
    .url-box {
      background: #111;
      border: 1px solid #2a2a4a;
      border-radius: 6px;
      padding: 0.75rem 1rem;
      font-family: monospace;
      font-size: 0.85rem;
      color: #7c8aff;
      word-break: break-all;
      margin: 0.5rem 0;
      text-align: center;
    }
    a { color: #7c8aff; text-decoration: none; }
    a:hover { text-decoration: underline; }
    hr { border: none; border-top: 1px solid #2a2a4a; margin: 1.5rem 0; }
  </style>
  <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
</head>
<body>
  <div class="container">
    <div class="logo">${PRODUCT_NAME}</div>
    <div class="subtitle">Web Flasher & OTA Update</div>

    <div class="card">
      <h3>Flash via USB</h3>
      <p>Connect your Murphy device via USB-C. Chrome/Edge required.</p>
      <ol class="steps">
        <li>Plug in your Murphy Reader via USB</li>
        <li>Click the button below</li>
        <li>Select the serial port and confirm</li>
        <li>Wait for the flash to complete (~60s)</li>
      </ol>
      <esp-web-install-button
        class="invisible"
        manifest="/manifest.json"
      ></esp-web-install-button>
      <p style="margin-top:1rem;font-size:0.85rem;color:#888;">
        Latest: v${FIRMWARE_VERSION} (${FIRMWARE_BUILD_DATE})
      </p>
    </div>

    <div class="card">
      <h3>OTA Update</h3>
      <p>Already connected to Wi-Fi? Update wirelessly.</p>
      <p style="color:#888;margin-top:0.5rem;">OTA endpoint:</p>
      <div class="url-box">https://murphy.pandacat.ai/ota/latest</div>
      <p style="margin-top:0.5rem;font-size:0.85rem;color:#888;">
        Your device will check this endpoint and auto-update.
      </p>
    </div>

    <hr>

    <div class="card">
      <h3>API Reference</h3>
      <p><code>GET /ota/latest</code> — Latest firmware info (JSON)</p>
      <p><code>GET /ota/check</code> — Simple version string</p>
      <p><code>GET /manifest.json</code> — ESP Web Tools manifest</p>
      <p><code>GET /firmware/${FIRMWARE_FILENAME}</code> — Firmware binary</p>
    </div>

    <div class="version">
      <a href="/changelog">📋 更新日誌</a> · ${PRODUCT_NAME} v${FIRMWARE_VERSION} · Build ${FIRMWARE_BUILD_DATE}
    </div>
  </div>
</body>
</html>`;
  return new Response(html, {
    headers: { "Content-Type": "text/html; charset=utf-8" },
  });
}

// ESP Web Tools manifest
function serveManifest(_env: Env): Response {
  const base = `https://murphy.pandacat.ai`;
  const manifest = {
    name: PRODUCT_NAME,
    version: FIRMWARE_VERSION,
    new_install_prompt_erase: true,
    new_install_improv_wait_time: 15,
    builds: [
      {
        chipFamily: CHIP_FAMILY,
        parts: [
          { path: `${base}/firmware/bootloader.bin`, offset: 0x0000 },
          { path: `${base}/firmware/partitions.bin`, offset: 0x8000 },
          { path: `${base}/firmware/boot_app0.bin`, offset: 0xe000 },
          { path: `${base}/firmware/${FIRMWARE_FILENAME}`, offset: APP_OFFSET },
        ],
      },
    ],
  };
  return new Response(JSON.stringify(manifest, null, 2), {
    headers: {
      "Content-Type": "application/json",
      "Access-Control-Allow-Origin": "*",
      "Cache-Control": "public, max-age=300",
    },
  });
}

// OTA latest version info
function serveOtaLatest(_env: Env): Response {
  const ota = {
    name: PRODUCT_NAME,
    version: FIRMWARE_VERSION,
    build_date: FIRMWARE_BUILD_DATE,
    chip_family: CHIP_FAMILY,
    firmware_url: `https://murphy.pandacat.ai/firmware/${FIRMWARE_FILENAME}`,
    app_offset: APP_OFFSET,
    force_update: false,
  };
  return new Response(JSON.stringify(ota, null, 2), {
    headers: {
      "Content-Type": "application/json",
      "Access-Control-Allow-Origin": "*",
      "Cache-Control": "no-cache",
    },
  });
}

// Simple version check: returns plain text version string
function serveOtaCheck(_env: Env): Response {
  return new Response(FIRMWARE_VERSION, {
    headers: {
      "Content-Type": "text/plain",
      "Access-Control-Allow-Origin": "*",
      "Cache-Control": "no-cache",
    },
  });
}

// Serve firmware binary from R2 bucket
async function serveFirmware(path: string, env: Env): Promise<Response> {
  // path is like `/firmware/murphy-26-0505-1.0.0.bin` or `/firmware/bootloader.bin`
  const key = path.replace("/firmware/", "");

  // Try R2 first, fall back to static response
  try {
    const object = await env.FIRMWARE_BUCKET.get(key);
    if (object) {
      const headers = new Headers();
      object.writeHttpMetadata(headers);
      headers.set("Access-Control-Allow-Origin", "*");
      headers.set("Cache-Control", "public, max-age=86400");
      headers.set("etag", object.httpEtag);
      return new Response(object.body, { headers });
    }
  } catch (e) {
    // R2 not configured yet — return 404
  }

  // List available firmware files
  return new Response(
    JSON.stringify({
      error: "firmware_not_found",
      message: `Firmware file '${key}' not found in storage. Upload it to the R2 bucket.`,
      hint: "Use: wrangler r2 object put murphy-firmware/murphy-26-0505-1.0.0.bin --file=.pio/build/mofei/firmware.bin",
    }),
    {
      status: 404,
      headers: {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "*",
      },
    }
  );
}

// Changelog HTML page
function renderChangelogHtml(entries: ChangelogEntry[]): string {
  const categoryLabel: Record<string, string> = {
    Added: "新增",
    Changed: "變更",
    Fixed: "修復",
    Removed: "移除",
  };

  const items = entries
    .map((entry) => {
      const changes = entry.changes
        .map(
          (c) =>
            `<div class="cat"><span class="cat-label cat-${c.category.toLowerCase()}">${
              categoryLabel[c.category]
            }</span><ul>${c.items.map((i) => `<li>${escapeHtml(i)}</li>`).join("")}</ul></div>`
        )
        .join("");
      return `<div class="release">
        <div class="rel-header">
          <span class="version">${escapeHtml(entry.version)}</span>
          <span class="date">${entry.date}</span>
        </div>
        ${changes}
      </div>`;
    })
    .join("");

  return `<!DOCTYPE html>
<html lang="zh-Hant">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>${PRODUCT_NAME} — 更新日誌</title>
  <style>
    *{margin:0;padding:0;box-sizing:border-box}
    body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","Microsoft YaHei","Noto Sans TC",Roboto,sans-serif;background:#0f0f1a;color:#e0e0e0;min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:2rem 1rem}
    .container{max-width:680px;width:100%}
    h1{font-size:1.5rem;color:#7c8aff;margin-bottom:.25rem}
    .subtitle{color:#8888aa;margin-bottom:2rem}
    .release{background:#1a1a2e;border-radius:12px;padding:1.5rem;margin-bottom:1rem;border:1px solid #2a2a4a}
    .rel-header{display:flex;align-items:baseline;gap:.75rem;margin-bottom:1rem}
    .version{font-size:1.25rem;font-weight:700;color:#7c8aff}
    .date{color:#666;font-size:.85rem}
    .cat{margin-bottom:.75rem}
    .cat-label{display:inline-block;font-size:.75rem;font-weight:600;padding:2px 8px;border-radius:4px;margin-bottom:.35rem}
    .cat-added{background:#1a3a1a;color:#4caf50}
    .cat-changed{background:#1a2a3a;color:#4da6ff}
    .cat-fixed{background:#3a2a1a;color:#ffa64d}
    .cat-removed{background:#3a1a1a;color:#ff4d4d}
    ul{list-style:none;padding-left:0}
    li{padding:.15rem 0;font-size:.9rem;color:#ccc;line-height:1.5}
    li::before{content:"– ";color:#555}
    a{color:#7c8aff;text-decoration:none}
    a:hover{text-decoration:underline}
    .back{display:inline-block;margin-top:1.5rem;font-size:.9rem}
    footer{text-align:center;color:#555;font-size:.8rem;margin-top:2rem}
  </style>
</head>
<body>
  <div class="container">
    <h1>${PRODUCT_NAME} — 更新日誌</h1>
    <div class="subtitle">Murphy Reader 韌體的所有重要變更記錄</div>
    ${items}
    <a class="back" href="/">&larr; 返回刷機頁面</a>
  </div>
  <footer>${PRODUCT_NAME} v${FIRMWARE_VERSION}</footer>
</body>
</html>`;
}

function escapeHtml(text: string): string {
  return text
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function serveChangelogHtml(): Response {
  return new Response(renderChangelogHtml(changelog), {
    headers: { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "public, max-age=3600" },
  });
}

function serveChangelogJson(): Response {
  return new Response(JSON.stringify(changelog, null, 2), {
    headers: {
      "Content-Type": "application/json",
      "Access-Control-Allow-Origin": "*",
      "Cache-Control": "public, max-age=3600",
    },
  });
}

// Handle CORS preflight
function handleOptions(_request: Request): Response {
  return new Response(null, {
    headers: {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
      "Access-Control-Allow-Headers": "Content-Type, Authorization",
      "Access-Control-Max-Age": "86400",
    },
  });
}

// Main request handler
export default {
  async fetch(request: Request, env: Env, _ctx: ExecutionContext): Promise<Response> {
    // CORS preflight
    if (request.method === "OPTIONS") {
      return handleOptions(request);
    }

    const url = new URL(request.url);
    const path = url.pathname;

    // Route: /
    if (path === "/" || path === "/index.html") {
      return serveIndex();
    }

    // Route: /changelog
    if (path === "/changelog") {
      return serveChangelogHtml();
    }

    // Route: /changelog.json
    if (path === "/changelog.json") {
      return serveChangelogJson();
    }

    // Route: /manifest.json
    if (path === "/manifest.json") {
      return serveManifest(env);
    }

    // Route: /ota/latest
    if (path === "/ota/latest") {
      return serveOtaLatest(env);
    }

    // Route: /ota/check
    if (path === "/ota/check") {
      return serveOtaCheck(env);
    }

    // Route: /version — alias for ota/check
    if (path === "/version") {
      return serveOtaCheck(env);
    }

    // Route: /firmware/*
    if (path.startsWith("/firmware/")) {
      return serveFirmware(path, env);
    }

    // 404
    return new Response(
      JSON.stringify({
        error: "not_found",
        endpoints: [
          { path: "/", description: "Web flasher page" },
          { path: "/changelog", description: "Release changelog" },
          { path: "/changelog.json", description: "Changelog JSON API" },
          { path: "/manifest.json", description: "ESP Web Tools manifest" },
          { path: "/ota/latest", description: "OTA latest version info (JSON)" },
          { path: "/ota/check", description: "OTA version string (plain text)" },
          { path: "/firmware/:file", description: "Firmware binary download" },
        ],
      }),
      {
        status: 404,
        headers: {
          "Content-Type": "application/json",
          "Access-Control-Allow-Origin": "*",
        },
      }
    );
  },
};
