const { chromium } = require('playwright');
const path = require('path');
const express = require('express');

(async () => {
  const app = express();
  app.use(express.static('vostok-nexus-v5-1/web'));
  const server = app.listen(3001);

  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.setViewportSize({ width: 390, height: 844 });

  try {
    await page.goto('http://localhost:3001');
    console.log('Premium UI Loaded');

    // Screenshot of improved Dashboard
    await page.screenshot({ path: 'v5.1_dashboard_final.png' });

    // Check for Glassmorphism
    const blur = await page.evaluate(() => {
        return window.getComputedStyle(document.querySelector('.glass-card')).backdropFilter;
    });
    console.log('Backdrop Filter:', blur);

  } catch (e) {
    console.error('UI Verification failed:', e);
  } finally {
    await browser.close();
    server.close();
  }
})();
