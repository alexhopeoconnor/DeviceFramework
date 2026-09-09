const fs = require("fs");
const path = require("path");

function artifactPath(...parts) {
  const root = process.env.ARTIFACT_DIR || "/artifacts";
  const target = path.join(root, ...parts);
  fs.mkdirSync(path.dirname(target), { recursive: true, mode: 0o700 });
  return target;
}

async function capture(page, name, options = {}) {
  await page.screenshot({
    path: artifactPath(name),
    fullPage: true,
    animations: "disabled",
    caret: "hide",
    ...options,
  });
}

function readmeMediaPath(...parts) {
  return artifactPath("readme-media", ...parts);
}

async function moveRecordedVideo(video, target) {
  const source = await video.path();
  fs.renameSync(source, target);
}

function writeArtifact(name, value) {
  fs.writeFileSync(artifactPath(name), `${JSON.stringify(value, null, 2)}\n`, { mode: 0o600 });
}

function attachBrowserDiagnostics(page, errors) {
  page.on("pageerror", (error) => errors.push({ type: "pageerror", message: error.message }));
  page.on("console", (message) => {
    if (message.type() === "error") {
      errors.push({ type: "console", message: message.text() });
    }
  });
}

function attachBrowserNetworkDiagnostics(page, events) {
  const pathFor = (url) => {
    try {
      return new URL(url).pathname;
    } catch {
      return "<unparseable-url>";
    }
  };

  // Keep only routing metadata. In particular, never retain headers, request
  // bodies, or query strings: a portal save request can contain credentials.
  page.on("response", (response) => {
    const request = response.request();
    events.push({
      type: "response",
      method: request.method(),
      path: pathFor(request.url()),
      status: response.status(),
    });
  });
  page.on("requestfailed", (request) => {
    events.push({
      type: "failed",
      method: request.method(),
      path: pathFor(request.url()),
      error: request.failure()?.errorText || "unknown error",
    });
  });
}

function readStationCredentials() {
  const envPath = process.env.DEVICE_UI_STATION_ENV;
  if (!envPath) return null;
  const values = {};
  for (const line of fs.readFileSync(envPath, "utf8").split(/\r?\n/)) {
    if (!line || line.startsWith("#")) continue;
    const separator = line.indexOf("=");
    if (separator > 0) values[line.slice(0, separator)] = line.slice(separator + 1);
  }
  const ssid = values.DEVICEFRAMEWORK_TEST_WIFI_SSID;
  const password = values.DEVICEFRAMEWORK_TEST_WIFI_PASSWORD;
  if (!ssid || !password) {
    throw new Error("DEVICE_UI_STATION_ENV must define DEVICEFRAMEWORK_TEST_WIFI_SSID and DEVICEFRAMEWORK_TEST_WIFI_PASSWORD.");
  }
  return { ssid, password };
}

async function waitForDeviceStatus(request) {
  let status;
  await require("@playwright/test").expect.poll(async () => {
    try {
      const response = await request.get("/api/status", {
        headers: {
          Authorization: `Basic ${Buffer.from(`${process.env.DEVICE_UI_USERNAME}:${process.env.DEVICE_UI_PASSWORD}`).toString("base64")}`,
        },
      });
      if (!response.ok()) return false;
      status = await response.json();
      return Boolean(status && status.hardware && status.runtime);
    } catch {
      return false;
    }
  }, { timeout: 20_000, intervals: [500, 800, 1_000] }).toBe(true);
  return status;
}

async function navigateDevicePage(page, path, selector) {
  for (let attempt = 0; attempt < 12; attempt += 1) {
    try {
      await page.goto(path, { waitUntil: "domcontentloaded" });
      if (await page.locator(selector).isVisible({ timeout: 1_500 })) return;
    } catch {
      // The ESP8266 can deliberately reject a concurrent low-priority request
      // while it completes a portal hand-off or closes WebSerial. Retrying the
      // same independent page request proves it recovers without hiding a
      // persistent failure.
    }
    await page.waitForTimeout(500);
  }
  throw new Error(`DeviceFramework page did not become ready: ${path}`);
}

module.exports = {
  artifactPath,
  attachBrowserDiagnostics,
  attachBrowserNetworkDiagnostics,
  capture,
  moveRecordedVideo,
  navigateDevicePage,
  readmeMediaPath,
  readStationCredentials,
  waitForDeviceStatus,
  writeArtifact,
};
