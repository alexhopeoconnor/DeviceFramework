const path = require("path");
const { defineConfig } = require("@playwright/test");

const artifactDir = process.env.ARTIFACT_DIR || path.join(__dirname, "artifacts");
const stationCredentialsMounted = Boolean(process.env.DEVICE_UI_STATION_ENV);

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
    // A station portal run has local Wi-Fi values in the DOM. The runner keeps
    // only its minimal temporary env file, but suppress all browser media in
    // that mode so a real SSID cannot be retained in a failure artifact.
    screenshot: stationCredentialsMounted ? "off" : "only-on-failure",
    // Portal form submissions include local Wi-Fi credentials. The host
    // supplies a minimal temporary env file and removes it after the run, but
    // a Playwright trace can record request bodies, so never retain one.
    trace: "off",
    video: stationCredentialsMounted ? "off" : "retain-on-failure",
    colorScheme: "light",
    locale: "en-AU",
    timezoneId: "UTC",
    reducedMotion: "reduce",
  },
});
