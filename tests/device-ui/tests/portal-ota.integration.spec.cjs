const fs = require("fs");
const { test, expect } = require("@playwright/test");
const {
  attachBrowserDiagnostics,
  attachBrowserNetworkDiagnostics,
  capture,
  writeArtifact,
} = require("./helpers.cjs");

async function readMarker(request) {
  try {
    const response = await request.get("/api/test/firmware-marker", { timeout: 2_000 });
    if (!response.ok()) return null;
    return await response.json();
  } catch {
    return null;
  }
}

async function waitForMarker(request, image) {
  let marker;
  await expect.poll(async () => {
    marker = await readMarker(request);
    return marker && marker.image === image ? marker : null;
  }, { timeout: 60_000, intervals: [500, 800, 1_000] }).not.toBeNull();
  return marker;
}

async function waitForAutomaticOutage(request) {
  await expect.poll(async () => (await readMarker(request)) === null, {
    timeout: 20_000,
    intervals: [250, 500, 800],
  }).toBe(true);
}

function waitForOtaRequestStart(page) {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      cleanup();
      reject(new Error("Timed out waiting for the portal OTA POST /u request to start."));
    }, 30_000);
    const onRequest = (request) => {
      const requestPath = new URL(request.url()).pathname;
      if (requestPath !== "/u" || request.method() !== "POST") return;
      cleanup();
      resolve();
    };
    const cleanup = () => {
      clearTimeout(timeout);
      page.off("request", onRequest);
    };
    page.on("request", onRequest);
  });
}

function expectedProtectedPortal() {
  const value = process.env.DEVICE_UI_OTA_EXPECTED_PORTAL_PROTECTED;
  if (value !== "true" && value !== "false") {
    throw new Error("DEVICE_UI_OTA_EXPECTED_PORTAL_PROTECTED must be true or false.");
  }
  return value === "true";
}

function assertMarker(marker, image, protectedPortal) {
  expect(marker).toMatchObject({
    schema: 1,
    image,
    portalProtected: protectedPortal,
    expectedPortalProtected: protectedPortal,
  });
  expect(Number.isInteger(marker.otaCapacity)).toBeTruthy();
  expect(marker.otaCapacity).toBeGreaterThan(0);
}

function unexpectedBrowserErrors(errors, firstPostUploadError) {
  return errors.filter((entry, index) => {
    if (index < firstPostUploadError) return true;
    // WiFiManager intentionally restarts after a successful POST /u. Only
    // narrow transport errors caused by that expected outage are acceptable
    // after the browser has dispatched the update request.
    return entry.type !== "console" || !/net::ERR_(ADDRESS_UNREACHABLE|CONNECTION_REFUSED|CONNECTION_RESET)/.test(entry.message);
  });
}

function waitForOtaTransport(page) {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      cleanup();
      reject(new Error("Timed out waiting for the portal OTA POST /u transport result."));
    }, 90_000);
    const isOtaRequest = (request) => {
      const requestPath = new URL(request.url()).pathname;
      return requestPath === "/u" && request.method() === "POST";
    };
    const cleanup = () => {
      clearTimeout(timeout);
      page.off("response", onResponse);
      page.off("requestfailed", onRequestFailed);
    };
    const onResponse = (response) => {
      if (!isOtaRequest(response.request())) return;
      cleanup();
      resolve({ type: "response", response });
    };
    const onRequestFailed = (request) => {
      if (!isOtaRequest(request)) return;
      cleanup();
      const failure = request.failure();
      const errorText = failure ? failure.errorText : "unknown error";
      // WiFiManager releases differ in whether they flush the success response
      // before restarting. This reset is valid only because the test below
      // also proves the actual A → outage → B sequence through fresh requests.
      if (errorText === "net::ERR_CONNECTION_RESET") {
        resolve({ type: "connection-reset", errorText });
        return;
      }
      reject(new Error(`Portal OTA POST /u failed before a response: ${errorText}`));
    };
    page.on("response", onResponse);
    page.on("requestfailed", onRequestFailed);
  });
}

test.describe("DeviceFramework portal HTTP OTA integration", () => {
  test.skip(process.env.DEVICE_UI_MODE !== "ota-portal", "Portal OTA test harness only.");

  test("uploads B through the actual portal form and requires an automatic A-to-B reboot", async ({ request, browser }) => {
    // This includes an observed outage plus two fresh post-reboot responses;
    // it intentionally exceeds the ordinary browser-suite per-test budget.
    test.setTimeout(300_000);
    const firmware = process.env.DEVICE_UI_OTA_FIRMWARE;
    const expectedImage = process.env.DEVICE_UI_OTA_EXPECTED_IMAGE;
    if (!firmware || !expectedImage) {
      throw new Error("Portal OTA test requires a mounted image B and expected marker.");
    }
    const firmwareSize = fs.statSync(firmware).size;
    expect(firmwareSize).toBeGreaterThan(0);
    expect(expectedImage).toBe("B");

    const protectedPortal = expectedProtectedPortal();
    const markerA = await waitForMarker(request, "A");
    assertMarker(markerA, "A", protectedPortal);
    // The host runner independently makes this same check before Docker is
    // started. Keep it in the browser test harness too so a changed mount cannot
    // bypass the fixture's real, running capacity report.
    expect(firmwareSize).toBeLessThanOrEqual(markerA.otaCapacity);

    const root = await request.get("/");
    expect(root.ok()).toBeTruthy();
    expect(await root.text()).toContain("<html");

    const context = await browser.newContext({ viewport: { width: 1440, height: 1080 } });
    const page = await context.newPage();
    const errors = [];
    const network = [];
    attachBrowserDiagnostics(page, errors);
    attachBrowserNetworkDiagnostics(page, network);

    await page.goto("/#/update", { waitUntil: "networkidle" });
    await expect(page.getByRole("heading", { name: "Update firmware", exact: true })).toBeVisible();
    // provisioningTitle applies to the portal's provisioning view. The update
    // view deliberately has a stable, built-in heading, so do not couple this
    // OTA test to unrelated branding placement.
    await expect(page.locator("#wm-ota-file")).toBeVisible();
    await page.locator("#wm-ota-file").setInputFiles(firmware);
    await capture(page, "ota-before-upload.png");

    // Start observing the host-side outage when the browser dispatches /u,
    // not after a response/reset notification. ESP32 can reboot and restore
    // the marker before a browser reports the failed request.
    const uploadStarted = waitForOtaRequestStart(page);
    const automaticOutage = uploadStarted.then(() => waitForAutomaticOutage(request));
    const uploadTransport = waitForOtaTransport(page);
    await page.locator("#wm-ota-form button[type=submit]").click();
    await expect(page.locator("#wm-ota-overlay")).toHaveAttribute("aria-hidden", "false");

    await uploadStarted;
    const uploadResult = await uploadTransport;
    if (uploadResult.type === "response") {
      expect(uploadResult.response.status()).toBe(200);
      expect(await uploadResult.response.json()).toMatchObject({
        ok: true,
        message: expect.stringMatching(/restarting/i),
      });
    } else {
      expect(uploadResult).toEqual({
        type: "connection-reset",
        errorText: "net::ERR_CONNECTION_RESET",
      });
    }

    const firstPostUploadError = errors.length;
    await automaticOutage;
    const markerB = await waitForMarker(request, expectedImage);
    assertMarker(markerB, "B", protectedPortal);
    // A second independent response makes a rebooted-but-immediately-crashing
    // B image fail rather than being accepted from one transient response.
    await new Promise((resolve) => setTimeout(resolve, 1_000));
    const markerBAgain = await waitForMarker(request, expectedImage);
    assertMarker(markerBAgain, "B", protectedPortal);

    await context.close();
    const unexpected = unexpectedBrowserErrors(errors, firstPostUploadError);
    writeArtifact("ota-result.json", {
      firmwareSize,
      protectedPortal,
      markerA,
      markerB,
      markerBAgain,
      uploadResult: uploadResult.type === "response"
        ? { type: "response", status: uploadResult.response.status() }
        : uploadResult,
      browserErrors: errors,
      unexpectedBrowserErrors: unexpected,
      network,
    });
    expect(unexpected).toEqual([]);
  });
});
