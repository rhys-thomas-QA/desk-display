/**
 * Non-secret config stored as JSON alongside the app.
 * Set values with: npm run configure -- config <key> <value>
 * Keys: monthly-limit, billing-reset-day
 */
import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';

const CONFIG_PATH = path.join(os.homedir(), '.desk-display.json');

interface Config {
  email: string;
  resolvedUserIds: Record<string, string>;  // email → user_id, persisted to avoid re-lookup
}

const DEFAULTS: Config = {
  email: '',
  resolvedUserIds: {},
};

export function readConfig(): Config {
  try {
    const raw = fs.readFileSync(CONFIG_PATH, 'utf-8');
    return { ...DEFAULTS, ...JSON.parse(raw) };
  } catch {
    return { ...DEFAULTS };
  }
}

export function writeConfig(patch: Partial<Config>): void {
  const current = readConfig();
  fs.writeFileSync(CONFIG_PATH, JSON.stringify({ ...current, ...patch }, null, 2));
}

export function getEmail(): string {
  return readConfig().email;
}

export function getCachedUserId(email: string): string | null {
  return readConfig().resolvedUserIds[email.toLowerCase()] ?? null;
}

export function setCachedUserId(email: string, userId: string): void {
  const current = readConfig();
  current.resolvedUserIds[email.toLowerCase()] = userId;
  fs.writeFileSync(CONFIG_PATH, JSON.stringify(current, null, 2));
}
