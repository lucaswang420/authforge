import { test, expect } from '@playwright/test';

test.describe('API Documentation (Swagger UI)', () => {
  const API_DOCS_URL = 'http://localhost:5555/docs/api/';

  // Swagger UI is stateful, run tests sequentially to avoid interference
  test.describe.configure({ mode: 'serial' });

  test('should load Swagger UI successfully', async ({ page }) => {
    await page.goto(API_DOCS_URL);
    
    // Check if the title is correct
    await expect(page).toHaveTitle(/OAuth2 Authorization Server API Documentation/);
    
    // Wait for the spec to load
    const infoTitle = page.locator('.info .title');
    await expect(infoTitle).toBeVisible({ timeout: 10000 });
    await expect(infoTitle).toContainText('OAuth2 Authorization Server API');
  });

  const testEndpoints = [
    { path: '/health', expectedStatus: /200/ },
    { path: '/.well-known/openid-configuration', expectedStatus: /200/ },
    { path: '/api/me', expectedStatus: /401/ }
  ];

  for (const endpoint of testEndpoints) {
    test(`should execute ${endpoint.path} via Try it out`, async ({ page }) => {
      await page.goto(API_DOCS_URL);

      // Find the specific endpoint operation block
      const opBlock = page.locator('.opblock').filter({ 
        has: page.locator('.opblock-summary-path').filter({ hasText: new RegExp(`^${endpoint.path.replace(/\//g, '\\/')}$`) })
      }).first();
      
      await opBlock.scrollIntoViewIfNeeded();
      
      // Expand it
      if (!await opBlock.evaluate(el => el.classList.contains('is-open'))) {
        await opBlock.locator('.opblock-summary').click();
      }

      // Click "Try it out"
      const tryItOutBtn = opBlock.getByRole('button', { name: 'Try it out' });
      if (await tryItOutBtn.isVisible()) {
        await tryItOutBtn.click();
        // Wait for animation
        await page.waitForTimeout(500);
      }

      // Click "Execute"
      const executeBtn = opBlock.getByRole('button', { name: 'Execute' });
      await executeBtn.click({ force: true });

      // Wait for the response status to appear in the live response section
      // In some versions of Swagger UI, it's inside .responses-wrapper
      const responseStatus = opBlock.locator('.live-responses-table .response-col_status, .responses-wrapper .response-col_status').first();
      await expect(responseStatus).toContainText(endpoint.expectedStatus, { timeout: 20000 });
      
      // Verify response body is present
      const responseBody = opBlock.locator('.live-responses-table .microlight').first();
      await expect(responseBody).toBeVisible();
    });
  }

  test('should list all major tags (System, OAuth2, Admin, User Profile, etc.)', async ({ page }) => {
    await page.goto(API_DOCS_URL);

    const tags = ['System', 'OAuth2', 'Admin', 'User Profile', 'MFA', 'WebAuthn'];
    for (const tag of tags) {
      const tagElement = page.locator('.opblock-tag').filter({ hasText: tag });
      await expect(tagElement.first()).toBeVisible();
    }
  });
  test('should find /oauth2/token endpoint and show its parameters', async ({ page }) => {
    await page.goto(API_DOCS_URL);

    // Find and expand the OAuth2 tag
    const oauth2Tag = page.locator('.opblock-tag').filter({ hasText: 'OAuth2' }).first();
    await oauth2Tag.scrollIntoViewIfNeeded();
    
    // Check if expanded (aria-expanded or checking if following sibling is visible)
    // A simple way is to click it if the endpoint isn't visible yet
    const tokenEndpoint = page.locator('.opblock-summary-path').filter({ hasText: '/oauth2/token' }).first();
    if (!await tokenEndpoint.isVisible()) {
      await oauth2Tag.click();
    }

    await tokenEndpoint.scrollIntoViewIfNeeded();
    await expect(tokenEndpoint).toBeVisible();
    await tokenEndpoint.click();

    // Check for some parameters
    const paramNames = ['grant_type', 'client_id', 'client_secret'];
    for (const name of paramNames) {
      const paramElement = page.locator('.parameter__name').filter({ hasText: name }).first();
      await expect(paramElement).toBeVisible({ timeout: 10000 });
    }
  });
});
