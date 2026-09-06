import fs from "node:fs";
import { createRequire } from "node:module";

const { expect, test } = createRequire(import.meta.url)("/opt/playwright/node_modules/@playwright/test");

const manifestPath = process.env.UI_MANIFEST || "/state/ui-manifest.json";
const resultPath = process.env.UI_RESULT || "/state/ui-result.json";
const expectInteraction = process.env.UI_EXPECT_INTERACTION === "1";

function manifest() {
  return JSON.parse(fs.readFileSync(manifestPath, "utf8"));
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
  const username = page.locator('input[name="username"]');
  if (!await isVisible(username)) {
    return false;
  }

  await username.fill(process.env.HA_TEST_USERNAME);
  await page.locator('input[name="password"]').fill(process.env.HA_TEST_PASSWORD);
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
    if (!String(error).includes("ERR_ABORTED")) {
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

    await expect(root).toBeVisible({ timeout: 30_000 });
    // HA's first-login redirect can abort the immediate navigation to the
    // private dashboard. Do not mistake the default Overview shell for it.
    if (await isVisible(dashboardTitle, 10_000)) {
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
  await expect(root).toHaveScreenshot("deviceframework-e2e-initial.png");

  if (!expectInteraction) {
    return;
  }

  const switchControl = root.getByRole("switch").first();
  await expect(switchControl).toBeVisible();
  await switchControl.click();
  await expect(root).toHaveScreenshot("deviceframework-e2e-switch-on.png");
  fs.writeFileSync(
    resultPath,
    JSON.stringify({ entity_id: ui.entities.switch, expected_state: "on" }, null, 2) + "\n",
    "utf8",
  );
});
