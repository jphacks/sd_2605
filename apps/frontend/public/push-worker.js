/* No fetch handler: account data, API responses and recordings are never cached here. */
self.addEventListener("install", event => event.waitUntil(self.skipWaiting()));
self.addEventListener("activate", event => event.waitUntil(self.clients.claim()));

self.addEventListener("push", event => {
  let message = {};
  try {
    const parsed = event.data ? event.data.json() : null;
    if (parsed && typeof parsed === "object") message = parsed;
  } catch { /* Show a generic notification. */ }
  const title = typeof message.title === "string" ? message.title : "おかえりんく";
  const body = typeof message.body === "string" ? message.body : "新しいお知らせが届きました。";
  let url = new URL("/history", self.location.origin);
  try {
    const candidate = new URL(message.url, self.location.origin);
    if (candidate.origin === self.location.origin && candidate.pathname === "/history") url = candidate;
  } catch { /* Retain the safe destination. */ }
  event.waitUntil(self.registration.showNotification(title, {
    body, icon: "/push-icon-192.png", badge: "/push-icon-192.png",
    tag: typeof message.tag === "string" ? message.tag : "okaerilink-event",
    data: { url: url.href }
  }));
});

self.addEventListener("notificationclick", event => {
  event.notification.close();
  let url = new URL("/history", self.location.origin);
  try {
    const candidate = new URL(event.notification.data?.url, self.location.origin);
    if (candidate.origin === self.location.origin && candidate.pathname === "/history") url = candidate;
  } catch { /* Retain the safe destination. */ }
  event.waitUntil((async () => {
    const windows = await self.clients.matchAll({ type: "window", includeUncontrolled: true });
    for (const client of windows) {
      if (new URL(client.url).origin === self.location.origin && "navigate" in client) {
        try {
          await client.navigate(url.href);
          return await client.focus();
        } catch { /* A stale window must not prevent opening the notification. */ }
      }
    }
    return self.clients.openWindow(url.href);
  })());
});
