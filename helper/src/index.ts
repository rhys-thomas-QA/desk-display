import express from 'express';
import type { Request, Response } from 'express';
import * as os from 'os';
import { createCipheriv, createHash, randomBytes, randomInt } from 'crypto';
import { Bonjour } from 'bonjour-service';
import keytar from 'keytar';
import { getCachedUserId, getEmail, setCachedUserId, writeConfig } from './config.js';
import { canReadEnterpriseAnalytics, getMonthlyUsage, resolveEnterpriseAnalyticsUserId } from './claude.js';
import { canReadOpenAICosts } from './openai.js';

const PORT = 3333;
const KEYCHAIN_SERVICE = 'desk-display';
const KEYCHAIN_ACCOUNT = 'anthropic-analytics-key';
const OPENAI_KEYCHAIN_ACCOUNT = 'openai-organization-key';
const MONTHLY_LIMIT_USD = 200;
const SETUP_CODE_DIGITS = 10;
const SETUP_CODE_TTL_MINUTES = 30;
const SETUP_CODE_TTL_MS = SETUP_CODE_TTL_MINUTES * 60 * 1000;
const SETUP_CODE = String(randomInt(0, 10 ** SETUP_CODE_DIGITS)).padStart(SETUP_CODE_DIGITS, '0');
const SETUP_CODE_EXPIRES_AT = Date.now() + SETUP_CODE_TTL_MS;
const SETUP_LOCKOUT_MS = 60 * 1000;
const MAX_SETUP_FAILURES = 8;
const PROVISIONING_KDF_PREFIX = 'desk-display provisioning v1';
const PROVISIONING_AAD = Buffer.from('desk-display-provision-v1', 'utf8');

let provisioned = false;
let failedSetupCodes = 0;
let setupLockedUntil = 0;

async function getApiKey(): Promise<string | null> {
  return keytar.getPassword(KEYCHAIN_SERVICE, KEYCHAIN_ACCOUNT);
}

async function getOpenAIKey(): Promise<string | null> {
  return keytar.getPassword(KEYCHAIN_SERVICE, OPENAI_KEYCHAIN_ACCOUNT);
}

async function isReady(): Promise<boolean> {
  const key = await getApiKey();
  const email = getEmail();
  if (!key || !email) return false;

  try {
    await canReadEnterpriseAnalytics(email, key);
    return true;
  } catch {
    return false;
  }
}

const SETUP_HTML = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Desk Display Setup</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #0d1117;
      color: #f0f6fc;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .card {
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 12px;
      padding: 32px;
      width: 100%;
      max-width: 420px;
    }
    h1 { font-size: 20px; color: #58a6ff; margin-bottom: 8px; }
    p.subtitle { font-size: 14px; color: #8b949e; margin-bottom: 28px; }
    label { display: block; font-size: 13px; color: #8b949e; margin-bottom: 6px; margin-top: 20px; }
    label:first-of-type { margin-top: 0; }
    input {
      width: 100%;
      padding: 9px 12px;
      background: #0d1117;
      border: 1px solid #30363d;
      border-radius: 6px;
      color: #f0f6fc;
      font-size: 14px;
      outline: none;
      transition: border-color 0.15s;
    }
    input:focus { border-color: #58a6ff; }
    .hint { font-size: 12px; color: #484f58; margin-top: 5px; }
    button {
      margin-top: 28px;
      width: 100%;
      padding: 10px;
      background: #58a6ff;
      color: #0d1117;
      border: none;
      border-radius: 6px;
      font-size: 15px;
      font-weight: 600;
      cursor: pointer;
      transition: opacity 0.15s;
    }
    button:hover { opacity: 0.85; }
    .error {
      background: #3d1c1c;
      border: 1px solid #ff7b72;
      border-radius: 6px;
      color: #ff7b72;
      font-size: 13px;
      padding: 10px 12px;
      margin-bottom: 20px;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Desk Display Setup</h1>
    <p class="subtitle">One-time setup. Credentials are stored securely on this machine.</p>
    {{ERROR}}
    <form method="POST" action="/setup">
      <label for="apiKey">Anthropic Analytics API Key</label>
      <input id="apiKey" type="password" name="apiKey" placeholder="sk-ant-..." required autocomplete="off" />
      <p class="hint">Use a Claude analytics key with read:analytics scope.</p>

      <label for="email">Your Claude Email Address</label>
      <input id="email" type="email" name="email" placeholder="you@company.com" required />

      <label for="openAiKey">OpenAI Organization Key <span style="color:#484f58;">(optional)</span></label>
      <input id="openAiKey" type="password" name="openAiKey" placeholder="sk-..." autocomplete="off" />
      <p class="hint">Use an OpenAI Admin/API key that can read organization costs.</p>

      <button type="submit">Save &amp; Continue</button>
    </form>
  </div>
</body>
</html>`;

const READY_HTML = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Desk Display — Ready</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #0d1117;
      color: #f0f6fc;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .card {
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 12px;
      padding: 32px;
      width: 100%;
      max-width: 440px;
    }
    h1 { font-size: 20px; color: #3fb950; margin-bottom: 8px; }
    p.subtitle { font-size: 14px; color: #8b949e; margin-bottom: 28px; line-height: 1.6; }
    .step {
      display: flex;
      gap: 12px;
      align-items: flex-start;
      margin-bottom: 16px;
      font-size: 14px;
      color: #c9d1d9;
      line-height: 1.5;
    }
    .step-num {
      background: #21262d;
      border: 1px solid #30363d;
      border-radius: 50%;
      width: 24px;
      height: 24px;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 12px;
      font-weight: 600;
      flex-shrink: 0;
      color: #58a6ff;
    }
    code {
      background: #0d1117;
      border: 1px solid #30363d;
      border-radius: 4px;
      padding: 2px 6px;
      font-size: 13px;
      color: #58a6ff;
    }
    .divider { border: none; border-top: 1px solid #21262d; margin: 24px 0; }
    .status {
      font-size: 13px;
      color: #8b949e;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #3fb950;
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.4; }
    }
    .success { color: #3fb950; font-weight: 600; font-size: 14px; }
    .setup-code {
      background: #0d1117;
      border: 1px solid #30363d;
      border-radius: 8px;
      color: #f0f6fc;
      font-size: 30px;
      font-weight: 700;
      letter-spacing: 6px;
      margin: 6px 0 18px;
      padding: 12px 16px;
      text-align: center;
    }
    a { color: #58a6ff; font-size: 13px; text-decoration: none; }
    a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Ready to provision</h1>
    <p class="subtitle">Your helper is ready to provision the display.</p>

    <div class="setup-code">{{SETUP_CODE}}</div>
    <p class="subtitle">This code is valid for {{SETUP_CODE_TTL_MINUTES}} minutes and is accepted once.</p>

    <div class="step">
      <div class="step-num">1</div>
      <div>Enter this setup code in the ESP32 setup portal.</div>
    </div>
    <div class="step">
      <div class="step-num">2</div>
      <div>Keep this page open until the display says provisioned or starts fetching.</div>
    </div>
    <div class="step">
      <div class="step-num">3</div>
      <div>The ESP32 will usually find this machine at <code>desk-display.local</code> and copy its setup.</div>
    </div>
    <div class="step">
      <div class="step-num">4</div>
      <div>After provisioning, the display updates on its own and you can close the helper.</div>
    </div>

    {{NETWORK_URLS}}

    <hr class="divider" />

    <div class="status" id="status">
      <div class="dot"></div>
      <span>Waiting for ESP32&hellip;</span>
    </div>

    <p style="margin-top: 20px;"><a href="/setup">Re-run setup</a></p>
  </div>

  <script>
    async function poll() {
      try {
        const res = await fetch('/provision-status');
        const { provisioned } = await res.json();
        if (provisioned) {
          document.getElementById('status').innerHTML =
            '<span class="success">Provisioned — you can close this window.</span>';
          return;
        }
      } catch {}
      setTimeout(poll, 2000);
    }
    poll();
  </script>
</body>
</html>`;

let mdnsStarted = false;
function startMdns() {
  if (mdnsStarted) return;
  mdnsStarted = true;
  const bonjour = new Bonjour();
  bonjour.publish({ name: 'Desk Display Helper', type: 'http', port: PORT, host: 'desk-display.local' });
  console.log('Advertising as desk-display.local via mDNS');
}

function localHelperUrls(): string[] {
  const urls = new Set<string>();
  for (const entries of Object.values(os.networkInterfaces())) {
    for (const entry of entries ?? []) {
      if (entry.family !== 'IPv4' || entry.internal) continue;
      urls.add(`http://${entry.address}:${PORT}`);
    }
  }
  return [...urls];
}

function normalizedRemoteAddress(req: Request): string {
  const address = req.socket.remoteAddress ?? '';
  return address.startsWith('::ffff:') ? address.slice(7) : address;
}

function localMachineAddresses(): Set<string> {
  const addresses = new Set(['127.0.0.1', '::1', 'localhost']);
  for (const entries of Object.values(os.networkInterfaces())) {
    for (const entry of entries ?? []) {
      addresses.add(entry.address);
    }
  }
  return addresses;
}

function requireLocalBrowser(req: Request, res: Response): boolean {
  if (localMachineAddresses().has(normalizedRemoteAddress(req))) return true;
  res.status(403).send('Open this setup page on the laptop running the helper app.');
  return false;
}

function readyHtml(): string {
  const urls = localHelperUrls();
  const fallback = urls.length
    ? `<div class="step">
        <div class="step-num">?</div>
        <div>If discovery fails, enter one of these helper addresses in the ESP setup portal: ${urls.map(url => `<code>${url}</code>`).join(' ')}</div>
      </div>`
    : '';
  return READY_HTML
    .replace('{{SETUP_CODE}}', SETUP_CODE)
    .replace('{{SETUP_CODE_TTL_MINUTES}}', String(SETUP_CODE_TTL_MINUTES))
    .replace('{{NETWORK_URLS}}', fallback);
}

function validSetupCode(value: unknown): boolean {
  return typeof value === 'string' && value.trim() === SETUP_CODE;
}

function provisioningEncryptionKey(): Buffer {
  return createHash('sha256')
    .update(PROVISIONING_KDF_PREFIX, 'utf8')
    .update(SETUP_CODE, 'utf8')
    .digest();
}

function encryptProvisioningPayload(payload: unknown): object {
  const iv = randomBytes(12);
  const cipher = createCipheriv('aes-256-gcm', provisioningEncryptionKey(), iv);
  cipher.setAAD(PROVISIONING_AAD);

  const ciphertext = Buffer.concat([
    cipher.update(JSON.stringify(payload), 'utf8'),
    cipher.final(),
  ]);
  const tag = cipher.getAuthTag();

  return {
    version: 1,
    alg: 'A256GCM',
    iv: iv.toString('base64'),
    tag: tag.toString('base64'),
    ciphertext: ciphertext.toString('base64'),
  };
}

function setupCodeExpired(): boolean {
  return Date.now() > SETUP_CODE_EXPIRES_AT;
}

function requireSetupCode(req: Request, res: Response): boolean {
  if (setupCodeExpired()) {
    res.status(410).json({ error: 'Setup code expired. Restart the helper app.' });
    return false;
  }

  if (Date.now() < setupLockedUntil) {
    res.status(429).json({ error: 'Too many invalid setup codes. Wait one minute and try again.' });
    return false;
  }

  if (!validSetupCode(req.get('x-setup-code'))) {
    failedSetupCodes++;
    if (failedSetupCodes >= MAX_SETUP_FAILURES) {
      setupLockedUntil = Date.now() + SETUP_LOCKOUT_MS;
      failedSetupCodes = 0;
    }
    res.status(403).json({ error: 'Invalid setup code' });
    return false;
  }

  failedSetupCodes = 0;
  setupLockedUntil = 0;
  return true;
}

async function main() {
  const app = express();
  app.disable('x-powered-by');
  app.use((_req, res, next) => {
    res.setHeader('Cache-Control', 'no-store');
    next();
  });
  app.use(express.urlencoded({ extended: false }));

  app.get('/', async (req, res) => {
    if (!requireLocalBrowser(req, res)) return;
    res.redirect(await isReady() ? '/ready' : '/setup');
  });

  app.get('/setup', (req, res) => {
    if (!requireLocalBrowser(req, res)) return;
    res.send(SETUP_HTML.replace('{{ERROR}}', ''));
  });

  app.post('/setup', async (req, res) => {
    if (!requireLocalBrowser(req, res)) return;
    const { apiKey, email, openAiKey } = req.body as {
      apiKey?: string;
      email?: string;
      openAiKey?: string;
    };

    if (!apiKey?.trim() || !email?.trim()) {
      res.send(SETUP_HTML.replace('{{ERROR}}', '<div class="error">Both fields are required.</div>'));
      return;
    }

    try {
      try {
        await canReadEnterpriseAnalytics(email.trim(), apiKey.trim());
      } catch {
        res.send(SETUP_HTML.replace('{{ERROR}}', '<div class="error">This key could not read Anthropic analytics. Check that it has read:analytics scope.</div>'));
        return;
      }

      const userId = await resolveEnterpriseAnalyticsUserId(email.trim(), apiKey.trim());
      if (!userId) {
        res.send(SETUP_HTML.replace('{{ERROR}}', '<div class="error">The key works, but this email was not found in recent Anthropic analytics data.</div>'));
        return;
      }

      const trimmedOpenAiKey = openAiKey?.trim() ?? '';
      if (trimmedOpenAiKey) {
        try {
          await canReadOpenAICosts(trimmedOpenAiKey);
        } catch {
          res.send(SETUP_HTML.replace('{{ERROR}}', '<div class="error">The OpenAI key could not read organization costs. Check that it has usage/costs access.</div>'));
          return;
        }
      }

      await keytar.setPassword(KEYCHAIN_SERVICE, KEYCHAIN_ACCOUNT, apiKey.trim());
      if (trimmedOpenAiKey) {
        await keytar.setPassword(KEYCHAIN_SERVICE, OPENAI_KEYCHAIN_ACCOUNT, trimmedOpenAiKey);
      } else {
        await keytar.deletePassword(KEYCHAIN_SERVICE, OPENAI_KEYCHAIN_ACCOUNT);
      }
      writeConfig({ email: email.trim() });
      setCachedUserId(email.trim(), userId);
      provisioned = false;

      const usage = await getMonthlyUsage(email.trim(), apiKey.trim(), MONTHLY_LIMIT_USD);
      if (usage.daysChecked === 0) {
        res.send(SETUP_HTML.replace('{{ERROR}}', '<div class="error">Setup saved, but no usage window was available yet.</div>'));
        return;
      }

      startMdns();
      res.redirect('/ready');
    } catch (err) {
      res.send(SETUP_HTML.replace('{{ERROR}}', `<div class="error">Setup failed: ${String(err)}</div>`));
    }
  });

  app.get('/ready', async (req, res) => {
    if (!requireLocalBrowser(req, res)) return;
    if (!await isReady()) { res.redirect('/setup'); return; }
    res.send(readyHtml());
  });

  app.get('/provision-status', (req, res) => {
    if (!requireLocalBrowser(req, res)) return;
    res.json({ provisioned });
  });

  app.get('/profile', (req, res) => {
    if (!requireSetupCode(req, res)) return;
    const email = getEmail();
    if (!email) { res.status(503).json({ error: 'Not configured' }); return; }
    res.json({ email });
  });

  app.get('/provision', async (req, res) => {
    if (!requireSetupCode(req, res)) return;
    if (provisioned) {
      res.status(409).json({ error: 'Already provisioned. Restart the helper app to provision again.' });
      return;
    }

    const apiKey = await getApiKey();
    const openAiKey = await getOpenAIKey();
    const email = getEmail();
    if (!apiKey || !email) { res.status(503).json({ error: 'Not configured' }); return; }

    try {
      await canReadEnterpriseAnalytics(email, apiKey);
    } catch {
      res.status(503).json({ error: 'Stored key cannot read analytics' });
      return;
    }

    let userId = getCachedUserId(email);
    if (!userId) {
      userId = await resolveEnterpriseAnalyticsUserId(email, apiKey);
      if (userId) setCachedUserId(email, userId);
    }
    if (!userId) {
      res.status(503).json({ error: 'User not resolved' });
      return;
    }

    res.json(encryptProvisioningPayload({
      api_key: apiKey,
      user_id: userId,
      email,
      openai_api_key: openAiKey || '',
    }));
    provisioned = true;
  });

  app.get('/usage', async (req, res) => {
    if (!requireSetupCode(req, res)) return;
    const apiKey = await getApiKey();
    const email = getEmail();
    if (!apiKey || !email) { res.status(503).json({ error: 'Helper setup needed' }); return; }

    try {
      const usage = await getMonthlyUsage(email, apiKey, MONTHLY_LIMIT_USD);
      res.json({
        email: usage.email,
        total_usd: usage.totalCents / 100,
        total_cents: usage.totalCents,
        limit_usd: usage.limitUsd,
        percent: usage.percent,
        currency: usage.currency,
        active_days: usage.activeDays,
        days_checked: usage.daysChecked,
        data_refreshed_at: usage.dataRefreshedAt,
        source: usage.source,
      });
    } catch (err) {
      console.warn('Usage fetch failed:', err);
      res.status(503).json({ error: 'Stored key cannot read analytics' });
    }
  });

  app.listen(PORT, '0.0.0.0', async () => {
    console.log(`\nDesk Display Helper running on :${PORT}`);
    console.log(`  Setup code: ${SETUP_CODE}`);

    if (await isReady()) {
      console.log(`  Ready to provision: http://localhost:${PORT}/ready`);
      startMdns();
    } else {
      console.log(`  Setup required: http://localhost:${PORT}/setup`);
    }
  });
}

main().catch(err => { console.error('Fatal:', err); process.exit(1); });
