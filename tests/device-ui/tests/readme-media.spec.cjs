const { test, expect } = require("@playwright/test");
const {
  attachBrowserDiagnostics,
  capture,
  moveRecordedVideo,
  navigateDevicePage,
  readmeMediaPath,
  waitForDeviceStatus,
} = require("./helpers.cjs");

async function waitForCompletedPortalScan(request) {
  await expect.poll(async () => {
    try {
      const response = await request.get("/api/wifi/scan-status");
      if (!response.ok()) return false;
      const result = JSON.parse(await response.text());
      return result.state === "complete" && result.results_valid && result.count > 0;
    } catch {
      return false;
    }
  }, { timeout: 45_000, intervals: [500, 800, 1_000] }).toBe(true);
}

function mediaRequested() {
  return process.env.DEVICE_UI_CAPTURE_README_MEDIA === "1"
    && process.env.DEVICE_UI_README_MEDIA_ONLY === "1";
}

test.describe("DeviceFramework README media", () => {
  test("records the ESP32 provisioning portal", async ({ browser, request }) => {
    test.skip(!mediaRequested() || process.env.DEVICE_UI_MODE !== "portal", "README portal capture was not requested.");

    const context = await browser.newContext({
      viewport: { width: 720, height: 900 },
      recordVideo: {
        dir: readmeMediaPath("raw"),
        size: { width: 720, height: 900 },
      },
    });
    const page = await context.newPage();
    const video = page.video();
    const errors = [];
    attachBrowserDiagnostics(page, errors);

    await page.goto("/", { waitUntil: "networkidle" });
    await expect(page.locator("#wm-reset-portal-timeout")).toBeVisible();
    // These pauses exist only in the README recording. The normal browser
    // contract remains timing-focused; a documentation tour needs readable
    // stable states after each meaningful transition.
    await page.waitForTimeout(1200);
    await page.locator('a[href="#/wifi"]').click();
    await expect(page.locator("#wm-refresh-scan")).toBeVisible();
    await waitForCompletedPortalScan(request);
    await expect(page.locator("#wm-scan-results .wm-scan-row").first()).toBeVisible();
    await page.waitForTimeout(1200);
    await context.close();

    await moveRecordedVideo(video, readmeMediaPath("raw", "portal-tour.webm"));
    expect(errors).toEqual([]);
  });

  test("records the ESP32 connected device UI", async ({ browser, request }) => {
    test.skip(!mediaRequested() || process.env.DEVICE_UI_MODE !== "web", "README web capture was not requested.");
    await waitForDeviceStatus(request);

    const context = await browser.newContext({
      viewport: { width: 720, height: 900 },
      recordVideo: {
        dir: readmeMediaPath("raw"),
        size: { width: 720, height: 900 },
      },
      httpCredentials: {
        username: process.env.DEVICE_UI_USERNAME,
        password: process.env.DEVICE_UI_PASSWORD,
      },
    });
    const page = await context.newPage();
    const video = page.video();
    const errors = [];
    const sockets = [];
    attachBrowserDiagnostics(page, errors);
    page.on("websocket", (socket) => sockets.push(socket));

    await navigateDevicePage(page, "/", '[data-df-page="status"]');
    await expect(page.getByRole("heading", { name: "Device Status", exact: true })).toBeVisible();
    await capture(page, "readme-media/device-status.png");
    await page.waitForTimeout(1200);

    await navigateDevicePage(page, "/serial", '[data-df-page="serial"]');
    await expect(page.locator("#webserial-status")).toBeVisible();
    await expect.poll(() => sockets.length, { timeout: 20_000 }).toBeGreaterThan(0);
    await page.waitForTimeout(1200);

    const controlsLink = page.locator('a[data-page="controls"]');
    if (!(await controlsLink.isVisible())) {
      await page.getByRole("button", { name: "Toggle navigation" }).click();
    }
    await controlsLink.click({ noWaitAfter: true });
    await expect.poll(() => sockets.filter((socket) => socket.isClosed()).length, {
      timeout: 10_000,
    }).toBeGreaterThan(0);
    await navigateDevicePage(page, "/controls", '[data-df-page="controls"]');
    await expect(page.locator("#device-password-form")).toBeVisible();
    await page.waitForTimeout(1200);
    await context.close();

    await moveRecordedVideo(video, readmeMediaPath("raw", "web-ui-tour.webm"));
    expect(errors).toEqual([]);
  });
});
