import { test, expect } from '@playwright/test'
import { setupMocks, loginViaForm } from './helpers/mock-api'

test.describe('Login', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
  })

  test('displays login form', async ({ page }) => {
    await page.goto('/login')
    await expect(page.getByRole('heading', { name: /sign in/i })).toBeVisible()
    await expect(page.locator('input[autocomplete="username"]')).toBeVisible()
    await expect(page.locator('input[autocomplete="current-password"]')).toBeVisible()
    await expect(page.locator('button[type="submit"]')).toBeVisible()
  })

  test('successful login redirects to dashboard', async ({ page }) => {
    await page.goto('/login')
    await page.locator('input[autocomplete="username"]').fill('testuser')
    await page.locator('input[autocomplete="current-password"]').fill('password123')
    await page.locator('button[type="submit"]').click()
    // After successful login, should navigate away from /login
    await expect(page).not.toHaveURL(/\/login$/, { timeout: 10000 })
  })

  test('shows error on invalid credentials', async ({ page }) => {
    await page.goto('/login')
    await page.locator('input[autocomplete="username"]').fill('testuser')
    await page.locator('input[autocomplete="current-password"]').fill('wrong')
    await page.locator('button[type="submit"]').click()
    await expect(page.locator('text=用户名或密码错误')).toBeVisible()
  })

  test('shows MFA challenge when required', async ({ page }) => {
    await page.route('**/oauth2/login', async (route) => {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ mfa_required: true, mfa_token: 'mfa-token-123' }) })
    })
    await page.goto('/login')
    await page.locator('input[autocomplete="username"]').fill('testuser')
    await page.locator('input[autocomplete="current-password"]').fill('password123')
    await page.locator('button[type="submit"]').click()
    await expect(page.locator('text=Two-Factor Authentication')).toBeVisible()
    await expect(page.locator('input[maxlength="6"]')).toBeVisible()
  })

  test('MFA verification completes login', async ({ page }) => {
    await page.route('**/oauth2/login', async (route) => {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ mfa_required: true, mfa_token: 'mfa-token-123' }) })
    })
    await page.goto('/login')
    await page.locator('input[autocomplete="username"]').fill('testuser')
    await page.locator('input[autocomplete="current-password"]').fill('password123')
    await page.locator('button[type="submit"]').click()
    // Should show MFA form
    await expect(page.locator('input[maxlength="6"]')).toBeVisible()
    await page.locator('input[maxlength="6"]').fill('123456')
    await page.locator('button[type="submit"]').click()
    // After MFA verify, should navigate to dashboard
    await expect(page).toHaveURL('/', { timeout: 10000 })
  })

  test('forgot password link navigates correctly', async ({ page }) => {
    await page.goto('/login')
    await page.click('a:has-text("Forgot password")')
    await expect(page).toHaveURL('/forgot-password')
  })

  test('register link navigates correctly', async ({ page }) => {
    await page.goto('/login')
    await page.click('a:has-text("create a new account")')
    await expect(page).toHaveURL('/register')
  })
})

test.describe('Register', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
  })

  test('displays registration form', async ({ page }) => {
    await page.goto('/register')
    await expect(page.getByRole('heading', { name: /create/i })).toBeVisible()
  })

  test('successful registration shows success message', async ({ page }) => {
    await page.goto('/register')
    await page.locator('input[autocomplete="username"]').fill('newuser')
    await page.locator('input[autocomplete="email"]').fill('new@example.com')
    const passwordFields = page.locator('input[type="password"]')
    await passwordFields.first().fill('StrongPass123')
    await passwordFields.nth(1).fill('StrongPass123')
    await page.locator('button[type="submit"]').click()
    await expect(page.locator('text=successfully')).toBeVisible()
  })
})

test.describe('Forgot Password', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
  })

  test('displays forgot password form', async ({ page }) => {
    await page.goto('/forgot-password')
    await expect(page.getByRole('heading', { name: /reset/i })).toBeVisible()
  })

  test('shows success message after submission', async ({ page }) => {
    await page.goto('/forgot-password')
    await page.locator('input[type="email"]').fill('test@example.com')
    await page.locator('button[type="submit"]').click()
    await expect(page.locator('text=If an account with that email exists')).toBeVisible()
  })
})

test.describe('Email Verification', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
  })

  test('shows success on valid token', async ({ page }) => {
    await page.goto('/verify-email?token=valid-token')
    await expect(page.getByRole('heading', { name: /verified/i })).toBeVisible()
  })

  test('shows error when token is missing', async ({ page }) => {
    await page.goto('/verify-email')
    await expect(page.locator('text=Missing')).toBeVisible()
  })
})

test.describe('GitHub Login', () => {
  test.beforeEach(async ({ page }) => {
    await setupMocks(page)
  })

  test('shows GitHub login button on login page', async ({ page }) => {
    await page.goto('/login')
    await expect(page.locator('text=Sign in with GitHub')).toBeVisible()
  })

  test('GitHub button links to GitHub OAuth', async ({ page }) => {
    await page.goto('/login')
    const githubLink = page.locator('a:has-text("Sign in with GitHub")')
    await expect(githubLink).toBeVisible()
    const href = await githubLink.getAttribute('href')
    expect(href).toContain('github.com/login/oauth/authorize')
  })
})
