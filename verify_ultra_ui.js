const { chromium } = require('playwright');
const express = require('express');

(async () => {
  const app = express();
  app.use(express.static('vostok-nexus-v5-1/web'));
  const server = app.listen(3002);

  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.setViewportSize({ width: 390, height: 844 });

  try {
    await page.goto('http://localhost:3002');
    console.log('Ultra Premium UI Loaded');

    // Screenshot of improved Russian Dashboard
    await page.screenshot({ path: 'v5.1_ultra_premium_final.png' });

    // Check for ECharts container
    const echartsExists = await page.evaluate(() => !!document.getElementById('profileChart'));
    console.log('ECharts Container Exists:', echartsExists);

  } catch (e) {
    console.error('UI Verification failed:', e);
  } finally {
    await browser.close();
    server.close();
  }
})();
