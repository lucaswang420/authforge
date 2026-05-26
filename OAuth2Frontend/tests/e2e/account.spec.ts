import { test, expect } from '@playwright/test'
import { setupMocks, loginUser } from './helpers/mock-api'

test.describe('Dashboard', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
    await loginUser(page)
  })

  test('displays welcome message', async ({ page }) => {
    await expect(page.locator('text=Welcome')).toBeVisible()
  })

  test('shows quick links', async ({ page }) => {
    await expect(page.locator('a:has-text("Edit Profile")')).toBeVisible()
    await expect(page.locator('a:has-text("Security Settings")')).toBeVisible()
    await expect(page.locator('a:has-text("Authorized Apps")')).toBeVisible()
  })

  test('quick links navigate correctly', async ({ page }) => {
    await page.click('a:has-text("Edit Profile")')
    await expect(page).toHaveURL('/profile')
  })
})

test.describe('Profile', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
    await loginUser(page)
    await page.click('nav a:has-text("Profile")')
    await page.waitForURL('/profile')
  })

  test('displays profile page', async ({ page }) => {
    await expect(page.getByRole('heading', { level: 1 })).toContainText('Profile')
  })

  test('shows user info', async ({ page }) => {
    await expect(page.locator('text=testuser')).toBeVisible()
    await expect(page.locator('text=test@example.com')).toBeVisible()
  })

  test('shows email verification status', async ({ page }) => {
    await expect(page.locator('text=Verified')).toBeVisible()
  })
})

test.describe('Security', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
    await loginUser(page)
    await page.click('nav a:has-text("Security")')
    await page.waitForURL('/security')
  })

  test('displays security page sections', async ({ page }) => {
    await expect(page.locator('text=Change Password')).toBeVisible()
    await expect(page.locator('text=Two-Factor Authentication')).toBeVisible()
  })

  test('can change password', async ({ page }) => {
    await page.locator('input[autocomplete="current-password"]').fill('oldpass')
    const newPassFields = page.locator('input[autocomplete="new-password"]')
    await newPassFields.first().fill('NewPass123!')
    await newPassFields.nth(1).fill('NewPass123!')
    await page.locator('button:has-text("Change Password")').click()
    await expect(page.locator('text=Password changed')).toBeVisible()
  })

  test('shows MFA enable button when disabled', async ({ page }) => {
    await expect(page.locator('button:has-text("Enable MFA")')).toBeVisible()
  })

  test('can start MFA setup', async ({ page }) => {
    await page.click('button:has-text("Enable MFA")')
    await expect(page.locator('text=JBSWY3DPEHPK3PXP')).toBeVisible()
  })
})

test.describe('Authorized Apps', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
    await loginUser(page)
    await page.click('nav a:has-text("Authorized Apps")')
    await page.waitForURL('/authorized-apps')
  })

  test('displays authorized apps', async ({ page }) => {
    await expect(page.locator('text=Third Party App')).toBeVisible()
    await expect(page.locator('text=Mobile App')).toBeVisible()
  })

  test('shows revoke buttons', async ({ page }) => {
    const revokeButtons = page.locator('button:has-text("Revoke")')
    await expect(revokeButtons).toHaveCount(2)
  })

  test('can revoke an app', async ({ page }) => {
    page.on('dialog', (dialog) => dialog.accept())
    await page.locator('button:has-text("Revoke")').first().click()
    await expect(page.locator('text=revoked')).toBeVisible()
  })
})
