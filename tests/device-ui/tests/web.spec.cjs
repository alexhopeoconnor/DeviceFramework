const { test, expect } = require("@playwright/test");
const { attachBrowserDiagnostics, capture, writeArtifact } = require("./helpers.cjs");

async function waitForStatus(request) {
  let status;
  await expect.poll(async () => {
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

async function navigateToPage(page, path, selector) {
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

test.describe("DeviceFramework board web UI", () => {
  test.skip(process.env.DEVICE_UI_MODE !== "web", "Board web contract only.");

  test("renders each server page and releases WebSerial on navigation", async ({ browser, request }) => {
    const status = await waitForStatus(request);
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

    await navigateToPage(page, "/", '[data-df-page="status"]');
    await expect(page.getByRole("heading", { name: "Device Status", exact: true })).toBeVisible();
    await capture(page, "status-desktop.png");

    await navigateToPage(page, "/serial", '[data-df-page="serial"]');
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
    await navigateToPage(page, "/controls", '[data-df-page="controls"]');
    await expect(page.locator("#device-password-form")).toBeVisible();
    await capture(page, "controls-desktop.png");

    await navigateToPage(page, "/about", '[data-df-page="about"]');
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
    await navigateToPage(mobilePage, "/", '[data-df-page="status"]');
    await capture(mobilePage, "status-mobile.png");
    await mobile.close();
    await context.close();

    writeArtifact("browser-console.json", errors);
    expect(errors).toEqual([]);
  });
});
