const BASE = 'https://api.openai.com';

function monthStartUnix(): number {
  const now = new Date();
  return Math.floor(Date.UTC(now.getUTCFullYear(), now.getUTCMonth(), 1) / 1000);
}

export async function canReadOpenAICosts(apiKey: string): Promise<void> {
  const url = new URL(`${BASE}/v1/organization/costs`);
  url.searchParams.set('start_time', String(monthStartUnix()));
  url.searchParams.set('limit', '1');

  const res = await fetch(url, {
    headers: {
      Authorization: `Bearer ${apiKey}`,
      'Content-Type': 'application/json',
    },
  });

  if (!res.ok) {
    const body = await res.text().catch(() => '');
    throw new Error(`OpenAI costs API returned ${res.status}: ${body.slice(0, 160)}`);
  }
}
