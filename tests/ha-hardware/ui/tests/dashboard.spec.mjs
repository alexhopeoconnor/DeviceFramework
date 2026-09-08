import fs from "node:fs";
import { createRequire } from "node:module";

const { expect, test } = createRequire(import.meta.url)("/opt/playwright/node_modules/@playwright/test");

const manifestPath = process.env.UI_MANIFEST || "/state/ui-manifest.json";
const resultPath = process.env.UI_RESULT || "/state/ui-result.json";
const expectInteraction = process.env.UI_EXPECT_INTERACTION === "1";
const captureDir = process.env.UI_CAPTURE_DIR || "/artifacts/screenshots";
const desktopViewport = { width: 1440, height: 1200 };
const mobileViewport = { width: 390, height: 844 };
// Playwright normalises screenshot names to filenames. Keep the version in the
// filename explicitly so the reviewed baseline is predictable and does not
// depend on slash/dot sanitisation performed by a particular Playwright build.
const snapshotPrefix = `ha-${(process.env.HA_UI_VERSION || "current").replace(/[^a-zA-Z0-9_-]/g, "-")}`;

function manifest() {
  return JSON.parse(fs.readFileSync(manifestPath, "utf8"));
}

function capturePath(name) {
  fs.mkdirSync(captureDir, { recursive: true, mode: 0o700 });
  return `${captureDir}/${name}`;
}

function navigationWasInterrupted(error) {
  return /ERR_ABORTED|interrupted by another navigation/.test(String(error));
}

async function capture(locator, name) {
  await locator.screenshot({
    path: capturePath(name),
    animations: "disabled",
    caret: "hide",
  });
}

async function capturePage(page, name) {
  fs.mkdirSync(captureDir, { recursive: true, mode: 0o700 });
  await page.screenshot({
    path: `${captureDir}/${name}`,
    fullPage: true,
    animations: "disabled",
    caret: "hide",
  });
}

async function isVisible(locator, timeout = 2_000) {
  try {
    await expect(locator).toBeVisible({ timeout });
    return true;
  } catch {
    return false;
  }
}

async function signInIfNeeded(page) {
  const username = page.getByRole("textbox", { name: /^Username\*?$/ });
  if (!await isVisible(username)) {
    return false;
  }

  await username.fill(process.env.HA_TEST_USERNAME);
  await page.getByRole("textbox", { name: /^Password\*?$/ }).fill(process.env.HA_TEST_PASSWORD);
  await page.getByRole("button", { name: /log in/i }).click();

  const finish = page.getByRole("button", { name: "Finish", exact: true });
  if (await isVisible(finish, 15_000)) {
    await finish.click();
    await expect(finish).toBeHidden({ timeout: 60_000 });
  }
  return true;
}

async function navigateToDashboard(page, dashboardUrl) {
  try {
    await page.goto(dashboardUrl, { waitUntil: "domcontentloaded" });
  } catch (error) {
    if (!navigationWasInterrupted(error)) {
      throw error;
    }
  }
}

async function dashboardRoot(page, dashboardUrl) {
  const root = page.locator("home-assistant");
  const dashboardTitle = root.getByText("DeviceFramework HA E2E", { exact: true });
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await navigateToDashboard(page, dashboardUrl);
    if (await signInIfNeeded(page)) {
      continue;
    }

    if (!await isVisible(root, 30_000)) {
      continue;
    }
    // HA's first-login redirect can abort the immediate navigation to the
    // private dashboard. Do not mistake the default Overview shell for it.
    if (await isVisible(dashboardTitle, 30_000)) {
      return root;
    }
  }
  throw new Error(`Home Assistant did not render the requested dashboard (current URL: ${page.url()})`);
}

test("DeviceFramework Home Assistant dashboard renders deterministically", async ({ page }) => {
  const ui = manifest();
  const root = await dashboardRoot(page, ui.dashboard_url);

  await expect(root.getByText("DeviceFramework HA E2E", { exact: true })).toBeVisible();
  for (const label of ["E2E Sensor", "E2E Switch", "E2E Number", "E2E Select", "E2E Text"]) {
    await expect(root.getByText(label, { exact: true })).toBeVisible();
  }
  const deviceControls = root.getByRole("heading", { name: "Device controls", exact: true }).locator("..");
  await expect(deviceControls).toBeVisible();
  await capturePage(page, "dashboard-desktop.png");
  await capture(deviceControls, "controls-dashboard-desktop.png");
  await expect(deviceControls).toHaveScreenshot(`${snapshotPrefix}-controls-dashboard-initial.png`);

  await page.setViewportSize(mobileViewport);
  await capturePage(page, "dashboard-mobile.png");

  if (!expectInteraction) {
    return;
  }

  // The reviewed state-change baseline is a desktop card. Restore the
  // desktop viewport after the independent mobile capture so a physical
  // browser round-trip compares the same layout as the fixture baseline.
  await page.setViewportSize(desktopViewport);

  // The entity name is asserted above. HA 2026.9 exposes the control without
  // propagating that name into its accessible role, so assert the fixed test
  // dashboard has one switch rather than relying on its shadow-DOM layout.
  const switchControl = root.getByRole("switch");
  await expect(switchControl).toHaveCount(1);
  await expect(switchControl).toBeVisible();
  // HA overlays its styled thumb over the native checkbox. Dispatch the click
  // to the accessible control while retaining the real frontend event path.
  await switchControl.click({ force: true });
  await capture(deviceControls, "controls-dashboard-switch-on.png");
  await expect(deviceControls).toHaveScreenshot(`${snapshotPrefix}-controls-dashboard-switch-on.png`);
  fs.writeFileSync(
    resultPath,
    JSON.stringify({ entity_id: ui.entities.switch, expected_state: "on" }, null, 2) + "\n",
    "utf8",
  );
});
