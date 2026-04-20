/**
 * Compute the next fire time for cron patterns used by this project.
 * Supports: "M H * * *", "M * * * *", "0 *\/<N> * * *"
 */
export default function nextCronRun(cronExpr, fromDate = new Date()) {
  const date = new Date(fromDate);
  date.setUTCSeconds(0, 0);
  date.setUTCMinutes(date.getUTCMinutes() + 1); // Start search from next minute

  const parts = cronExpr.trim().split(/\s+/);
  if (parts.length !== 5) return null;

  const [minute, hour, dom, mon, dow] = parts;

  // Pattern: "M H * * *" — daily at minute M, hour H
  if (/^\d+$/.test(minute) && /^\d+$/.test(hour) && dom === '*' && mon === '*' && dow === '*') {
    const m = parseInt(minute);
    const h = parseInt(hour);
    if (m < 0 || m > 59 || h < 0 || h > 23) return null;

    for (let i = 0; i < 366; i++) {
      if (date.getUTCHours() === h && date.getUTCMinutes() === m) return new Date(date);
      date.setUTCDate(date.getUTCDate() + 1);
      date.setUTCHours(0, 0, 0, 0);
    }
    return null;
  }

  // Pattern: "M * * * *" — hourly at minute M
  if (/^\d+$/.test(minute) && hour === '*' && dom === '*' && mon === '*' && dow === '*') {
    const m = parseInt(minute);
    if (m < 0 || m > 59) return null;

    for (let i = 0; i < 366 * 24; i++) {
      if (date.getUTCMinutes() === m) return new Date(date);
      date.setUTCMinutes(date.getUTCMinutes() + 1);
    }
    return null;
  }

  // Pattern: "0 */<N> * * *" — every N hours at minute 0
  if (minute === '0' && /^\*\/\d+$/.test(hour) && dom === '*' && mon === '*' && dow === '*') {
    const n = parseInt(hour.slice(2));
    if (n < 1 || n > 23) return null;

    date.setUTCMinutes(0);
    const startHour = date.getUTCHours();
    for (let i = 0; i < 366 * 24; i++) {
      if (date.getUTCHours() % n === 0) return new Date(date);
      date.setUTCHours(date.getUTCHours() + 1);
    }
    return null;
  }

  return null;
}

// Sanity test
if (import.meta.url === `file://${process.argv[1]}`) {
  const now = new Date();
  console.log('nextCronRun sanity test:');
  console.log('15 * * * * (at :15):', nextCronRun('15 * * * *', now));
  console.log('30 * * * * (at :30):', nextCronRun('30 * * * *', now));
  console.log('0 */2 * * * (every 2h):', nextCronRun('0 */2 * * *', now));
}
