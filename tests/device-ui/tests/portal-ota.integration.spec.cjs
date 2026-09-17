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
    version: image === "A" ? "0.0.0-portal-ota-a" : "0.0.0-portal-ota-b",
    portalProtected: protectedPortal,
    expectedPortalProtected: protectedPortal,
  });
  expect(Number.isInteger(marker.otaCapacity)).toBeTruthy();
  expect(marker.otaCapacity).toBeGreaterThan(0);
}

function unexpectedBrowserErrors(errors, firstPostUploadError) {
  return errors.filter((entry, index) => {
    if (index < firstPostUploadError) return true;
    // WiFiManager intentionally restarts after replying successfully to POST
    // /u. Only the narrow transport errors caused by that expected outage are
    // acceptable after the browser has observed the success JSON.
    return entry.type !== "console" || !/net::ERR_(ADDRESS_UNREACHABLE|CONNECTION_REFUSED|CONNECTION_RESET)/.test(entry.message);
  });
}

test.describe("DeviceFramework portal HTTP OTA integration", () => {
  test.skip(process.env.DEVICE_UI_MODE !== "ota-portal", "Portal OTA contract only.");

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
    // started. Keep it in the browser contract too so a changed mount cannot
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
    await expect(page.getByText("Update DeviceFramework OTA Test", { exact: true })).toBeVisible();
    await expect(page.locator("#wm-ota-file")).toBeVisible();
    await page.locator("#wm-ota-file").setInputFiles(firmware);
    await capture(page, "ota-before-upload.png");

    const uploadResponse = page.waitForResponse((response) => {
      const requestForResponse = response.request();
      return new URL(response.url()).pathname === "/u" && requestForResponse.method() === "POST";
    });
    await page.locator("#wm-ota-form button[type=submit]").click();
    await expect(page.locator("#wm-ota-overlay")).toHaveAttribute("aria-hidden", "false");

    const response = await uploadResponse;
    expect(response.status()).toBe(200);
    expect(await response.json()).toMatchObject({
      ok: true,
      message: expect.stringMatching(/restarting/i),
    });

    const firstPostUploadError = errors.length;
    await waitForAutomaticOutage(request);
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
      browserErrors: errors,
      unexpectedBrowserErrors: unexpected,
      network,
    });
    expect(unexpected).toEqual([]);
  });
});
