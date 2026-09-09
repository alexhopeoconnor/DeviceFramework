const { test, expect } = require("@playwright/test");
const {
  attachBrowserDiagnostics,
  attachBrowserNetworkDiagnostics,
  capture,
  readStationCredentials,
  writeArtifact,
} = require("./helpers.cjs");

async function json(response) {
  return JSON.parse(await response.text());
}

async function waitForCompletedScan(request) {
  let state;
  let sawActive = false;
  await expect.poll(async () => {
    try {
      const response = await request.get("/api/wifi/scan-status");
      if (!response.ok()) return true;
      state = await json(response);
      if (state.state === "complete" && state.results_valid) return true;
      sawActive ||= state.scanning;
      return sawActive && !state.scanning;
    } catch {
      // ESP8266 may leave its AP channel while scanning. The client should
      // reconnect before the completed result is checked below; do not let a
      // transient route loss turn a previous completed scan into a false pass.
      return false;
    }
  }, { timeout: 45_000, intervals: [500, 800, 1_000] }).toBe(true);
  return state;
}

function assertSaveDiagnostics(errors, network) {
  // A successful AP-to-station switch can cut the browser off mid-poll. Keep
  // that transport-level Chrome message separate from application errors;
  // the host runner then proves the board appeared on the station network.
  const expectedHandoffTransport = errors.filter((entry) =>
    entry.type === "console" && entry.message === "Failed to load resource: net::ERR_ADDRESS_UNREACHABLE");
  const unexpected = errors.filter((entry) => !expectedHandoffTransport.includes(entry));
  writeArtifact("browser-save-console.json", {
    all: errors,
    expectedDuringHandoff: expectedHandoffTransport,
    unexpected,
  });
  writeArtifact("browser-network.json", network);
  expect(unexpected).toEqual([]);
}

test.describe("DeviceFramework WiFiManager integration", () => {
  test.skip(process.env.DEVICE_UI_MODE !== "portal", "Portal contract only.");

  test("launches DeviceFramework branding through the real provisioning portal", async ({ request, browser }) => {
    const root = await request.get("/");
    expect(root.ok()).toBeTruthy();
    expect(await root.text()).toContain("<html");

    const bootstrapResponse = await request.get("/api/bootstrap");
    expect(bootstrapResponse.ok()).toBeTruthy();
    const bootstrap = await json(bootstrapResponse);
    expect(bootstrap.contractVersion).toBe(3);
    expect(bootstrap.context.portalActive).toBe(true);
    expect(bootstrap.brand.title).toBe("Set up DeviceFramework UI Test");
    expect(bootstrap.context.identityText).toBe("Test Lab");
    expect(bootstrap.brand.tagline).toContain("Connected-device portal");

    const metaResponse = await request.get("/api/wifi/meta");
    expect(metaResponse.ok()).toBeTruthy();
    expect(JSON.stringify(await json(metaResponse))).toContain("Device Name");

    const timeoutReset = await request.post("/api/portal/timeout-reset");
    expect(timeoutReset.ok()).toBeTruthy();
    expect((await json(timeoutReset)).timeoutSecondsRemaining).toBeGreaterThan(80);

    const desktop = await browser.newContext({ viewport: { width: 1440, height: 1080 } });
    const page = await desktop.newPage();
    const errors = [];
    const network = [];
    attachBrowserDiagnostics(page, errors);
    attachBrowserNetworkDiagnostics(page, network);
    await page.goto("/", { waitUntil: "networkidle" });
    await expect(page.locator("#wm-reset-portal-timeout")).toBeVisible();
    await expect(page.getByText("Set up DeviceFramework UI Test", { exact: true })).toBeVisible();
    await capture(page, "overview-desktop.png");

    await page.locator('a[href="#/wifi"]').click();
    await expect(page.locator("#wm-refresh-scan")).toBeVisible();
    const diagnosticsBeforeScan = errors.length;
    const scanOverlay = page.locator("#wm-wifi-scan-overlay");
    // Opening the Wi-Fi view starts a fresh scan. Complete that automatic
    // operation first; clicking while it is entering/leaving the SDK scan
    // state races the embedded radio rather than testing a user interaction.
    const automaticScan = await waitForCompletedScan(request);
    expect(automaticScan.state).toBe("complete");
    expect(automaticScan.results_valid).toBe(true);
    expect(automaticScan.count).toBeGreaterThan(0);
    await expect(page.locator("#wm-scan-results .wm-scan-row").first()).toBeVisible();

    // Then exercise a deliberate user refresh after the configured restart
    // interval. This both proves the button path and produces the retained
    // in-progress screenshot without overlapping the automatic scan.
    await page.waitForTimeout(1_000);
    await page.locator("#wm-refresh-scan").click();
    await expect(scanOverlay).toBeVisible();
    await capture(page, "scan-in-progress.png");
    const scan = await waitForCompletedScan(request);
    expect(scan.state).toBe("complete");
    expect(scan.results_valid).toBe(true);
    expect(scan.count).toBeGreaterThan(0);
    // ESP8266 temporarily vacates the soft-AP channel during a multi-channel
    // station scan. The browser can therefore report a transient route failure
    // while its normal polling retries; prove the page recovers, then retain
    // that narrowly-scoped transport diagnostic separately from real errors.
    await expect(page.locator("#wm-refresh-scan")).toBeVisible({ timeout: 20_000 });
    await capture(page, "wifi-desktop.png");

    const mobile = await browser.newContext({ viewport: { width: 390, height: 844 }, isMobile: true });
    const mobilePage = await mobile.newPage();
    attachBrowserDiagnostics(mobilePage, errors);
    attachBrowserNetworkDiagnostics(mobilePage, network);
    await mobilePage.goto("/", { waitUntil: "networkidle" });
    await expect(mobilePage.locator("#wm-reset-portal-timeout")).toBeVisible();
    await capture(mobilePage, "overview-mobile.png");
    await mobilePage.goto("/#/info", { waitUntil: "networkidle" });
    await expect(mobilePage.locator(".wm-page-head")).toBeVisible();
    await capture(mobilePage, "info-mobile.png");

    await mobile.close();
    await desktop.close();
    const scanTransportErrors = errors.slice(diagnosticsBeforeScan).filter((entry) =>
      entry.type === "console" && entry.message === "Failed to load resource: net::ERR_ADDRESS_UNREACHABLE");
    const unexpectedErrors = errors.filter((entry, index) =>
      index < diagnosticsBeforeScan || !scanTransportErrors.includes(entry));
    writeArtifact("browser-console.json", {
      all: errors,
      expectedDuringEsp8266Scan: scanTransportErrors,
      unexpected: unexpectedErrors,
    });
    writeArtifact("browser-network-launch.json", network);
    expect(unexpectedErrors).toEqual([]);
  });

  test("shows save progress, then provisions a station network through the portal UI", async ({ request, browser }) => {
    const station = readStationCredentials();
    test.skip(!station, "No ignored local station environment was supplied.");

    const metaResponse = await request.get("/api/wifi/meta");
    expect(metaResponse.ok()).toBeTruthy();
    const meta = await json(metaResponse);
    const deviceName = process.env.DEVICE_UI_TEST_DEVICE_NAME;
    const profileMode = Array.isArray(meta.profiles) && meta.profiles.length;
    const context = await browser.newContext({ viewport: { width: 1440, height: 1080 } });
    const page = await context.newPage();
    const errors = [];
    const network = [];
    attachBrowserDiagnostics(page, errors);
    attachBrowserNetworkDiagnostics(page, network);

    await page.goto("/#/wifi", { waitUntil: "networkidle" });
    const ssidInput = page.locator(profileMode ? "#wm-s0" : "#wm-f-s");
    const passwordInput = page.locator(profileMode ? "#wm-p0" : "#wm-f-p");
    await expect(ssidInput).toBeVisible();
    await expect(passwordInput).toBeVisible();
    await ssidInput.fill(station.ssid);
    await passwordInput.fill(station.password);
    if (deviceName) {
      const deviceInput = page.locator("#wm-f-device");
      if (await deviceInput.count()) await deviceInput.fill(deviceName);
    }

    const saveOverlay = page.locator("#wm-wifi-save-overlay");
    await page.locator('[data-wm-wifi-action="connect"]').click();
    await expect(saveOverlay).toBeVisible();
    await expect(saveOverlay).toHaveAttribute("aria-hidden", "false");
    await expect(ssidInput).toBeDisabled();
    await capture(page, "save-in-progress.png");

    let state;
    const deadline = Date.now() + 20_000;
    while (Date.now() < deadline) {
      let response;
      try {
        // The AP can disappear as soon as the ESP32 changes radio channel for
        // the station network. Bound this one request so that expected route
        // loss immediately hands control back to the host runner, which then
        // verifies the board on its station address.
        response = await request.get("/api/wifi/connect-status", { timeout: 3_000 });
      } catch (error) {
        // ESP32 can move its one radio from the portal AP channel to the
        // selected station channel before the browser receives the final JSON.
        // The host-side harness follows the deterministic mDNS name next; it
        // still fails if the board never joins the station network.
        writeArtifact("portal-handoff.json", {
          state: "portal-transport-interrupted",
          message: String(error.message || error),
          deviceName: deviceName || null,
        });
        await context.close();
        assertSaveDiagnostics(errors, network);
        return;
      }
      if (!response.ok()) throw new Error(`portal status returned HTTP ${response.status()}`);
      state = await json(response);
      if (state.state === "failed") {
        throw new Error(`portal reported Wi-Fi failure: ${state.message || "unknown error"}`);
      }
      if (state.state === "success") {
        expect(state.stationIp).toMatch(/^\d+\.\d+\.\d+\.\d+$/);
        expect(state.redirectUrl).toContain(state.stationIp);
        writeArtifact("portal-handoff.json", {
          state: "success",
          stationIp: state.stationIp,
          redirectUrl: state.redirectUrl,
        });
        await context.close();
        assertSaveDiagnostics(errors, network);
        return;
      }
      await new Promise((resolve) => setTimeout(resolve, 700));
    }

    await context.close();
    assertSaveDiagnostics(errors, network);
    throw new Error(
      `portal did not complete the station hand-off within 20 seconds (last state: ${state?.state || "unavailable"})`,
    );
  });
});
