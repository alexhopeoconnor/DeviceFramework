import fs from "node:fs";
import { createRequire } from "node:module";

const { expect, test } = createRequire(import.meta.url)("/opt/playwright/node_modules/@playwright/test");

const manifestPath = process.env.UI_MANIFEST || "/state/ui-manifest.json";
const captureDir = process.env.UI_CAPTURE_DIR || "/artifacts/screenshots";

function manifest() {
  return JSON.parse(fs.readFileSync(manifestPath, "utf8"));
}

function navigationWasInterrupted(error) {
  return /ERR_ABORTED|interrupted by another navigation/.test(String(error));
}

async function visible(locator, timeout = 2_000) {
  try {
    await expect(locator).toBeVisible({ timeout });
    return true;
  } catch {
    return false;
  }
}

async function signInIfNeeded(page) {
  const username = page.getByRole("textbox", { name: /^Username\*?$/ });
  if (!await visible(username)) return false;
  await username.fill(process.env.HA_TEST_USERNAME);
  await page.getByRole("textbox", { name: /^Password\*?$/ }).fill(process.env.HA_TEST_PASSWORD);
  await page.getByRole("button", { name: /log in/i }).click();
  const finish = page.getByRole("button", { name: "Finish", exact: true });
  if (await visible(finish, 15_000)) {
    await finish.click();
    await expect(finish).toBeHidden({ timeout: 60_000 });
  }
  return true;
}

async function goToDevicePage(page, ui) {
  const deviceUrl = `http://homeassistant:8123/config/devices/device/${encodeURIComponent(ui.ha_device_id)}`;
  for (let attempt = 0; attempt < 3; attempt += 1) {
    try {
      await page.goto(deviceUrl, { waitUntil: "domcontentloaded" });
    } catch (error) {
      if (!navigationWasInterrupted(error)) throw error;
    }
    if (await signInIfNeeded(page)) continue;
    if (await visible(page.getByText(ui.device_name, { exact: true }), 30_000)) return;
  }
  throw new Error(`Home Assistant did not render native device page for ${ui.device_name}`);
}

test("DeviceFramework Home Assistant native device page renders discovered entities", async ({ page }) => {
  const ui = manifest();
  await goToDevicePage(page, ui);

  await expect(page.getByText(ui.device_name, { exact: true })).toBeVisible();
  for (const label of ["E2E Sensor", "E2E Switch", "E2E Number", "E2E Select", "E2E Text"]) {
    // HA repeats an entity name in its logbook/history panel. The leading
    // detail row is the stable native device-page representation we need to
    // prove exists; later repetitions are not separate DeviceFramework data.
    await expect(page.getByText(label, { exact: true }).first()).toBeVisible();
  }

  fs.mkdirSync(captureDir, { recursive: true, mode: 0o700 });
  await page.screenshot({
    path: `${captureDir}/device-page-desktop.png`,
    fullPage: true,
    animations: "disabled",
    caret: "hide",
  });

  // The native page includes HA's live activity timestamps. Retain a full
  // evidence image, but keep this assertion semantic rather than accepting a
  // deliberately unstable full-page pixel baseline.
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({
    path: `${captureDir}/device-page-mobile.png`,
    fullPage: true,
    animations: "disabled",
    caret: "hide",
  });
});
