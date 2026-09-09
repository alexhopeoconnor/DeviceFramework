const { test, expect } = require("@playwright/test");
const {
  attachBrowserDiagnostics,
  capture,
  navigateDevicePage,
  waitForDeviceStatus,
  writeArtifact,
} = require("./helpers.cjs");

test.describe("DeviceFramework board web UI", () => {
  test.skip(process.env.DEVICE_UI_MODE !== "web", "Board web contract only.");

  test("renders each server page and releases WebSerial on navigation", async ({ browser, request }) => {
    const status = await waitForDeviceStatus(request);
    writeArtifact("status.json", status);

    const context = await browser.newContext({
      viewport: { width: 1440, height: 1080 },
      httpCredentials: {
        username: process.env.DEVICE_UI_USERNAME,
        password: process.env.DEVICE_UI_PASSWORD,
      },
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
      httpCredentials: {
        username: process.env.DEVICE_UI_USERNAME,
        password: process.env.DEVICE_UI_PASSWORD,
      },
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
