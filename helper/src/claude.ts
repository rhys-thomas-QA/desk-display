const BASE = 'https://api.anthropic.com';
const ANTHROPIC_VERSION = '2023-06-01';

export interface MonthlyUsage {
  email: string;
  totalCents: number;
  percent: number;
  limitUsd: number;
  currency: 'USD';
  daysChecked: number;
  activeDays: number;
  dataRefreshedAt: string;
  source: 'enterprise_analytics' | 'admin_claude_code';
}

async function fetchJson(
  url: string,
  apiKey: string,
  options: { adminApi?: boolean } = {},
): Promise<unknown> {
  const headers: Record<string, string> = {
    'x-api-key': apiKey,
    'User-Agent': 'DeskDisplay/1.0',
  };

  if (options.adminApi) {
    headers['anthropic-version'] = ANTHROPIC_VERSION;
  }

  const res = await fetch(url, { headers });
  if (!res.ok) {
    const text = await res.text();
    throw new Error(`Anthropic API ${res.status}: ${text}`);
  }
  return res.json();
}

function isoNoMillis(date: Date): string {
  return date.toISOString().replace(/\.\d{3}Z$/, 'Z');
}

function utcDateStr(date: Date): string {
  const y = date.getUTCFullYear();
  const m = String(date.getUTCMonth() + 1).padStart(2, '0');
  const d = String(date.getUTCDate()).padStart(2, '0');
  return `${y}-${m}-${d}`;
}

function currentUtcMonthWindow(): { start: Date; end: Date } {
  const now = new Date();
  return {
    start: new Date(Date.UTC(now.getUTCFullYear(), now.getUTCMonth(), 1)),
    end: now,
  };
}

function currentUtcMonthDates(): string[] {
  const { start, end } = currentUtcMonthWindow();
  const day = new Date(start);
  const today = utcDateStr(end);
  const dates: string[] = [];

  while (true) {
    const dateStr = utcDateStr(day);
    dates.push(dateStr);
    if (dateStr === today) break;
    day.setUTCDate(day.getUTCDate() + 1);
  }

  return dates;
}

function percentOfLimit(totalCents: number, limitUsd: number): number {
  const limitCents = limitUsd * 100;
  return Math.min(100, Math.round((totalCents / limitCents) * 100));
}

function actorEmail(record: Record<string, unknown>): string | null {
  const actor = record['actor'] as Record<string, unknown> | undefined;
  if (typeof actor?.['email_address'] === 'string') return actor['email_address'] as string;
  if (typeof actor?.['email'] === 'string') return actor['email'] as string;
  return null;
}

function amountCents(value: unknown): number {
  if (typeof value === 'number') return value;
  if (typeof value === 'string') {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : 0;
  }
  return 0;
}

function adminRecordCostCents(record: Record<string, unknown>): number {
  const models = record['model_breakdown'] as Record<string, unknown>[] | undefined;
  if (!Array.isArray(models)) return 0;

  let total = 0;
  for (const model of models) {
    const cost = model['estimated_cost'] as Record<string, unknown> | undefined;
    total += amountCents(cost?.['amount']);
  }
  return total;
}

export async function getEnterpriseAnalyticsUsage(
  email: string,
  apiKey: string,
  limitUsd: number,
): Promise<MonthlyUsage> {
  const normalizedEmail = email.toLowerCase();
  const { start, end } = currentUtcMonthWindow();
  let totalCents = 0;
  let matchedRows = 0;
  let cursor: string | undefined;
  let refreshedAt = isoNoMillis(end);

  while (true) {
    const url = new URL(`${BASE}/v1/organizations/analytics/user_cost_report`);
    url.searchParams.set('starting_at', isoNoMillis(start));
    url.searchParams.set('ending_at', isoNoMillis(end));
    url.searchParams.set('order_by', 'amount');
    url.searchParams.set('limit', '1000');
    if (cursor) url.searchParams.set('page', cursor);

    const data = await fetchJson(url.toString(), apiKey) as {
      data?: Record<string, unknown>[];
      has_more?: boolean;
      next_page?: string | null;
      data_refreshed_at?: string;
    };

    if (data.data_refreshed_at) refreshedAt = data.data_refreshed_at;

    for (const record of data.data ?? []) {
      if (actorEmail(record)?.toLowerCase() !== normalizedEmail) continue;
      totalCents += amountCents(record['amount']);
      matchedRows++;
    }

    if (!data.has_more || !data.next_page) break;
    cursor = data.next_page;
  }

  const daysChecked = currentUtcMonthDates().length;
  return {
    email,
    totalCents,
    percent: percentOfLimit(totalCents, limitUsd),
    limitUsd,
    currency: 'USD',
    daysChecked,
    activeDays: matchedRows > 0 ? daysChecked : 0,
    dataRefreshedAt: refreshedAt,
    source: 'enterprise_analytics',
  };
}

async function getAdminClaudeCodeUsage(
  email: string,
  apiKey: string,
  limitUsd: number,
): Promise<MonthlyUsage> {
  const normalizedEmail = email.toLowerCase();
  const dates = currentUtcMonthDates();
  let totalCents = 0;
  let activeDays = 0;

  for (const dateStr of dates) {
    let cursor: string | undefined;
    let foundForDay = false;

    while (true) {
      const url = new URL(`${BASE}/v1/organizations/usage_report/claude_code`);
      url.searchParams.set('starting_at', dateStr);
      url.searchParams.set('limit', '1000');
      if (cursor) url.searchParams.set('page', cursor);

      const data = await fetchJson(url.toString(), apiKey, { adminApi: true }) as {
        data?: Record<string, unknown>[];
        next_page?: string | null;
      };

      for (const record of data.data ?? []) {
        if (actorEmail(record)?.toLowerCase() !== normalizedEmail) continue;
        totalCents += adminRecordCostCents(record);
        foundForDay = true;
      }

      if (!data.next_page) break;
      cursor = data.next_page;
    }

    if (foundForDay) activeDays++;
  }

  return {
    email,
    totalCents,
    percent: percentOfLimit(totalCents, limitUsd),
    limitUsd,
    currency: 'USD',
    daysChecked: dates.length,
    activeDays,
    dataRefreshedAt: isoNoMillis(new Date()),
    source: 'admin_claude_code',
  };
}

export async function getMonthlyUsage(
  email: string,
  apiKey: string,
  limitUsd = 200,
): Promise<MonthlyUsage> {
  try {
    return await getEnterpriseAnalyticsUsage(email, apiKey, limitUsd);
  } catch (enterpriseErr) {
    try {
      return await getAdminClaudeCodeUsage(email, apiKey, limitUsd);
    } catch (adminErr) {
      throw new Error(
        `Key cannot read Anthropic analytics. Enterprise analytics: ${enterpriseErr}; Admin analytics: ${adminErr}`,
      );
    }
  }
}

export async function canReadUsageAnalytics(
  email: string,
  apiKey: string,
): Promise<boolean> {
  await getMonthlyUsage(email, apiKey, 200);
  return true;
}

export async function canReadEnterpriseAnalytics(
  email: string,
  apiKey: string,
): Promise<boolean> {
  await getEnterpriseAnalyticsUsage(email, apiKey, 200);
  return true;
}

export async function resolveEnterpriseAnalyticsUserId(
  email: string,
  apiKey: string,
): Promise<string | null> {
  const normalizedEmail = email.toLowerCase();
  const today = new Date();

  for (let daysBack = 3; daysBack <= 31; daysBack++) {
    const date = new Date(Date.UTC(
      today.getUTCFullYear(),
      today.getUTCMonth(),
      today.getUTCDate() - daysBack,
    ));
    const dateStr = utcDateStr(date);
    let cursor: string | undefined;

    while (true) {
      const url = new URL(`${BASE}/v1/organizations/analytics/users`);
      url.searchParams.set('date', dateStr);
      url.searchParams.set('limit', '1000');
      if (cursor) url.searchParams.set('page', cursor);

      const data = await fetchJson(url.toString(), apiKey) as {
        data?: Record<string, unknown>[];
        next_page?: string | null;
      };

      for (const record of data.data ?? []) {
        const user = record['user'] as Record<string, unknown> | undefined;
        const rowEmail = user?.['email_address'];
        const userId = user?.['id'];
        if (typeof rowEmail !== 'string' || rowEmail.toLowerCase() !== normalizedEmail) continue;
        return typeof userId === 'string' && userId ? userId : null;
      }

      if (!data.next_page) break;
      cursor = data.next_page;
    }
  }

  return null;
}
