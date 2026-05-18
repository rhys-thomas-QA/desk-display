import express from 'express';
import * as os from 'os';
import { randomInt } from 'crypto';
import { Bonjour } from 'bonjour-service';
import keytar from 'keytar';
import { getCachedUserId, getEmail, setCachedUserId, writeConfig } from './config.js';
import { canReadEnterpriseAnalytics, getMonthlyUsage, resolveEnterpriseAnalyticsUserId } from './claude.js';

const PORT = 3333;
const KEYCHAIN_SERVICE = 'desk-display';
const KEYCHAIN_ACCOUNT = 'anthropic-analytics-key';
const MONTHLY_LIMIT_USD = 200;
const SETUP_CODE = String(randomInt(0, 1000000)).padStart(6, '0');

let provisioned = false;

async function getApiKey(): Promise<string | null> {
  return keytar.getPassword(KEYCHAIN_SERVICE, KEYCHAIN_ACCOUNT);
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
    .replace('{{NETWORK_URLS}}', fallback);
}

function validSetupCode(value: unknown): boolean {
  return typeof value === 'string' && value.trim() === SETUP_CODE;
}

async function main() {
  const app = express();
  app.use(express.urlencoded({ extended: false }));

  app.get('/', async (_req, res) => {
    res.redirect(await isReady() ? '/ready' : '/setup');
  });

  app.get('/setup', (_req, res) => {
    res.send(SETUP_HTML.replace('{{ERROR}}', ''));
  });

  app.post('/setup', async (req, res) => {
    const { apiKey, email } = req.body as { apiKey?: string; email?: string };

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

      await keytar.setPassword(KEYCHAIN_SERVICE, KEYCHAIN_ACCOUNT, apiKey.trim());
      writeConfig({ email: email.trim() });
      setCachedUserId(email.trim(), userId);

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

  app.get('/ready', async (_req, res) => {
    if (!await isReady()) { res.redirect('/setup'); return; }
    res.send(readyHtml());
  });

  app.get('/provision-status', (_req, res) => {
    res.json({ provisioned });
  });

  app.get('/profile', (_req, res) => {
    const email = getEmail();
    if (!email) { res.status(503).json({ error: 'Not configured' }); return; }
    res.json({ email });
  });

  app.get('/provision', async (req, res) => {
    if (!validSetupCode(req.query.code)) {
      res.status(403).json({ error: 'Invalid setup code' });
      return;
    }

    const apiKey = await getApiKey();
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

    provisioned = true;
    res.json({ api_key: apiKey, user_id: userId, email });
  });

  app.get('/usage', async (_req, res) => {
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
