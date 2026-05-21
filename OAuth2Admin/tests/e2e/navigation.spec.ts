import { test, expect } from '@playwright/test'
import { setupAuthenticatedMocks, loginAsAdmin } from './helpers/mock-api'

test.describe('Navigation & Layout', () => {
  test.beforeEach(async ({ page }) => {
    await setupAuthenticatedMocks(page)
    await loginAsAdmin(page)
  })

  test('sidebar shows all navigation items', async ({ page }) => {
    await expect(page.locator('nav a:has-text("Dashboard")')).toBeVisible()
    await expect(page.locator('nav a:has-text("Applications")')).toBeVisible()
    await expect(page.locator('nav a:has-text("Users")')).toBeVisible()
    await expect(page.locator('nav a:has-text("Audit Logs")')).toBeVisible()
    await expect(page.locator('nav a:has-text("Tokens")')).toBeVisible()
    await expect(page.locator('nav a:has-text("Settings")')).toBeVisible()
  })

  test('sidebar navigation works for all pages', async ({ page }) => {
    // Applications
    await page.click('nav a:has-text("Applications")')
    await expect(page).toHaveURL(/\/admin\/applications/)
    await expect(page.locator('h2')).toContainText('Applications')

    // Users
    await page.click('nav a:has-text("Users")')
    await expect(page).toHaveURL(/\/admin\/users/)
    await expect(page.locator('h2')).toContainText('Users')

    // Audit Logs
    await page.click('nav a:has-text("Audit Logs")')
    await expect(page).toHaveURL(/\/admin\/logs/)
    await expect(page.locator('h2')).toContainText('Audit Logs')

    // Settings
    await page.click('nav a:has-text("Settings")')
    await expect(page).toHaveURL(/\/admin\/settings/)
    await expect(page.locator('h2')).toContainText('Settings')

    // Dashboard
    await page.click('nav a:has-text("Dashboard")')
    await expect(page).toHaveURL(/\/admin\/$/)
    await expect(page.locator('h2')).toContainText('Dashboard')
  })

  test('top bar shows user info', async ({ page }) => {
    // The sidebar user section shows the user's email
    await expect(page.locator('aside').getByText('admin@example.com')).toBeVisible()
  })

  test('logout clears session and redirects to login', async ({ page }) => {
    // Logout button says "Sign out" in the sidebar
    const logoutBtn = page.locator('button:has-text("Sign out")')
    await expect(logoutBtn).toBeVisible()
    await logoutBtn.click()
    await expect(page).toHaveURL(/\/admin\/login/)
  })

  test('active nav item is highlighted', async ({ page }) => {
    // Navigate to Applications and check active state
    await page.click('nav a:has-text("Applications")')
    const activeLink = page.locator('nav a:has-text("Applications")')
    // Active links have bg-gray-800 class
    await expect(activeLink).toHaveClass(/bg-gray-800/)
  })
})
