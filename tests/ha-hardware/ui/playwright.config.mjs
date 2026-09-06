import { createRequire } from "node:module";

const { defineConfig } = createRequire(import.meta.url)("/opt/playwright/node_modules/@playwright/test");
const artifactRoot = process.env.ARTIFACT_DIR || "/artifacts";

export default defineConfig({
  testDir: "./tests",
  outputDir: `${artifactRoot}/playwright`,
  reporter: [["list"], ["json", { outputFile: `${artifactRoot}/playwright/results.json` }]],
  retries: 0,
  timeout: 90_000,
  expect: {
    timeout: 30_000,
    toHaveScreenshot: {
      animations: "disabled",
      caret: "hide",
      maxDiffPixelRatio: 0.001,
    },
  },
  snapshotPathTemplate: "{testDir}/snapshots/{arg}{ext}",
  use: {
    browserName: "chromium",
    headless: true,
    viewport: { width: 1440, height: 1200 },
    colorScheme: "light",
    locale: "en-AU",
    timezoneId: "UTC",
    reducedMotion: "reduce",
    screenshot: "only-on-failure",
    trace: "retain-on-failure",
    video: "off",
    launchOptions: { args: ["--no-sandbox"] },
  },
});
