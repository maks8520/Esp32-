const { chromium } = require('playwright');
const path = require('path');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.setViewportSize({ width: 390, height: 844 }); // iPhone size

  // Start server
  const { exec } = require('child_process');
  const server = exec('node web/server.js');

  await new Promise(r => setTimeout(r, 2000));

  try {
    await page.goto('http://localhost:3000');
    console.log('Page loaded');

    // Take screenshot of Dashboard
    await page.screenshot({ path: 'dashboard.png' });
    console.log('Dashboard screenshot taken');

    // Switch to Meteo tab
    await page.click('button[data-tab="meteo"]');
    await new Promise(r => setTimeout(r, 1000));
    await page.screenshot({ path: 'meteo.png' });
    console.log('Meteo screenshot taken');

  } catch (e) {
    console.error('UI Verification failed:', e);
  } finally {
    await browser.close();
    server.kill();
  }
})();
