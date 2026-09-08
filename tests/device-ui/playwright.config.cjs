const path = require("path");
const { defineConfig } = require("@playwright/test");

const artifactDir = process.env.ARTIFACT_DIR || path.join(__dirname, "artifacts");

module.exports = defineConfig({
  testDir: "./tests",
  timeout: 60_000,
  expect: { timeout: 15_000 },
  forbidOnly: Boolean(process.env.CI),
  fullyParallel: false,
  workers: 1,
  outputDir: path.join(artifactDir, "test-results"),
  reporter: [
    ["list"],
    ["json", { outputFile: path.join(artifactDir, "report.json") }],
    ["html", { outputFolder: path.join(artifactDir, "html-report"), open: "never" }],
  ],
  use: {
    baseURL: process.env.DEVICE_UI_URL,
    screenshot: "only-on-failure",
    trace: "retain-on-failure",
    video: "retain-on-failure",
    colorScheme: "light",
    locale: "en-AU",
    timezoneId: "UTC",
    reducedMotion: "reduce",
  },
});
