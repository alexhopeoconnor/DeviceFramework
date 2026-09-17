const { test, expect } = require("@playwright/test");
const {
  attachBrowserDiagnostics,
  browserHttpCredentials,
  capture,
  navigateDevicePage,
  waitForDeviceStatus,
  webAuthMode,
  writeArtifact,
} = require("./helpers.cjs");

async function assertWebAuthBoundary(request) {
  const authMode = webAuthMode();
  const root = await request.get("/");
  const status = await request.get("/api/status");
  const stylesheet = await request.get("/assets/deviceframework.css");
  if (authMode === "protected") {
    expect(root.status()).toBe(401);
    expect(status.status()).toBe(401);
    expect(stylesheet.status()).toBe(401);
    const wrongCredentials = await request.get("/api/status", {
      headers: {
        Authorization: `Basic ${Buffer.from("admin:wrong-device-ui-password").toString("base64")}`,
      },
    });
    expect(wrongCredentials.status()).toBe(401);
  } else {
    expect(root.status()).toBe(200);
    expect(status.status()).toBe(200);
    expect(stylesheet.status()).toBe(200);
  }
}

test.describe("DeviceFramework board web UI", () => {
  test.skip(process.env.DEVICE_UI_MODE !== "web", "Board web test harness only.");

  test("renders each server page and releases WebSerial on navigation", async ({ browser, request }) => {
    await assertWebAuthBoundary(request);
    const status = await waitForDeviceStatus(request);
    if (process.env.DEVICE_UI_TEST_DEVICE_NAME) {
      expect(status.runtime.device.name).toBe(process.env.DEVICE_UI_TEST_DEVICE_NAME);
    }
    writeArtifact("status.json", status);

    const context = await browser.newContext({
      viewport: { width: 1440, height: 1080 },
      ...browserHttpCredentials(),
    });
    const page = await context.newPage();
    const errors = [];
    const sockets = [];
    attachBrowserDiagnostics(page, errors);
    page.on("websocket", (socket) => sockets.push(socket));

    await navigateDevicePage(page, "/", '[data-df-page="status"]');
    await expect(page.getByRole("heading", { name: "Device Status", exact: true })).toBeVisible();
    await capture(page, "status-desktop.png");

    await navigateDevicePage(page, "/serial", '[data-df-page="serial"]');
    await expect(page.locator("#webserial-status")).toBeVisible();
    await expect.poll(() => sockets.length, { timeout: 20_000 }).toBeGreaterThan(0);
    await capture(page, "serial-desktop.png");

    // Exercise the normal same-tab link, rather than forcing a page goto: the
    // client deliberately sends a WebSocket close before it starts the next
    // page request. Then use bounded recovery only for that independent HTTP
    // request, which can transiently meet the embedded admission policy.
    await page.locator('a[data-page="controls"]').click({ noWaitAfter: true });
    await expect.poll(() => sockets.filter((socket) => socket.isClosed()).length, {
      timeout: 10_000,
    }).toBeGreaterThan(0);
    await navigateDevicePage(page, "/controls", '[data-df-page="controls"]');
    await expect(page.locator("#device-password-form")).toBeVisible();
    await capture(page, "controls-desktop.png");

    await navigateDevicePage(page, "/about", '[data-df-page="about"]');
    await expect(page.getByText("Test Lab", { exact: true })).toBeVisible();
    await capture(page, "about-desktop.png");

    const mobile = await browser.newContext({
      viewport: { width: 390, height: 844 },
      isMobile: true,
      ...browserHttpCredentials(),
    });
    const mobilePage = await mobile.newPage();
    attachBrowserDiagnostics(mobilePage, errors);
    await navigateDevicePage(mobilePage, "/", '[data-df-page="status"]');
    await capture(mobilePage, "status-mobile.png");
    await mobile.close();
    await context.close();

    writeArtifact("browser-console.json", errors);
    expect(errors).toEqual([]);
  });
});
