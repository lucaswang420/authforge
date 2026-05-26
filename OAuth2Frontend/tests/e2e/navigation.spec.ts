import { test, expect } from '@playwright/test'
import { setupMocks, loginUser, loginViaForm } from './helpers/mock-api'

test.describe('Navigation & Route Guards', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
  })

  test('unauthenticated user is redirected to login', async ({ page }) => {
    await page.goto('/')
    await expect(page).toHaveURL(/\/login/)
  })

  test('unauthenticated user can access login page', async ({ page }) => {
    await page.goto('/login')
    await expect(page.getByRole('heading', { name: /sign in/i })).toBeVisible()
  })

  test('unauthenticated user can access register page', async ({ page }) => {
    await page.goto('/register')
    await expect(page.getByRole('heading', { name: /create/i })).toBeVisible()
  })

  test('authenticated user is redirected from login to dashboard', async ({ page }) => {
    await loginUser(page)
    await page.goto('/login')
    await expect(page).toHaveURL('/')
  })

  test('top navigation shows all links when authenticated', async ({ page }) => {
    await loginUser(page)
    await expect(page.locator('nav a:has-text("Overview")')).toBeVisible()
    await expect(page.locator('nav a:has-text("Profile")')).toBeVisible()
    await expect(page.locator('nav a:has-text("Security")')).toBeVisible()
    await expect(page.locator('nav a:has-text("Authorized Apps")')).toBeVisible()
  })

  test('navigation links work correctly', async ({ page }) => {
    await loginUser(page)

    await page.click('nav a:has-text("Profile")')
    await expect(page).toHaveURL('/profile')

    await page.click('nav a:has-text("Security")')
    await expect(page).toHaveURL('/security')

    await page.click('nav a:has-text("Authorized Apps")')
    await expect(page).toHaveURL('/authorized-apps')

    await page.click('nav a:has-text("Overview")')
    await expect(page).toHaveURL('/')
  })

  test('sign out clears session and redirects to login', async ({ page }) => {
    await loginUser(page)
    // Open user menu dropdown first
    await page.locator('header button:has(div.rounded-full)').click()
    await page.click('button:has-text("Sign Out")')
    await expect(page).toHaveURL(/\/login/)
  })

  test('after sign out, protected routes redirect to login', async ({ page }) => {
    await loginUser(page)
    await page.locator('header button:has(div.rounded-full)').click()
    await page.click('button:has-text("Sign Out")')
    await page.waitForURL(/\/login/)
    await page.goto('/profile')
    await expect(page).toHaveURL(/\/login/)
  })
})
