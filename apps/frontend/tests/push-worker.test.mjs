import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";

const script = readFileSync(new URL("../public/push-worker.js", import.meta.url), "utf8");
function worker(windows = []) {
  const listeners = {}, shown = [], opened = [];
  const context = {
    URL,
    self: {
      location: { origin: "https://app.example" },
      addEventListener(name, listener) { listeners[name] = listener; },
      skipWaiting: async () => {},
      registration: { showNotification: async (title, options) => { shown.push({ title, options }); } },
      clients: {
        claim: async () => {}, matchAll: async () => windows,
        openWindow: async url => { opened.push(url); }
      }
    }
  };
  vm.runInNewContext(script, context);
  async function fire(name, event) {
    let work;
    listeners[name]({ ...event, waitUntil(promise) { work = promise; } });
    await work;
  }
  return { fire, shown, opened, listeners };
}

test("push always displays a visible notification with a safe history destination", async () => {
  const w = worker();
  await w.fire("push", { data: { json: () => ({ title: "帰宅しました", body: "返信があります", tag: "event-1", url: "/history?device_id=entrance-1&event_id=abc" }) } });
  assert.equal(w.shown[0].title, "帰宅しました");
  assert.equal(w.shown[0].options.tag, "event-1");
  assert.equal(w.shown[0].options.data.url, "https://app.example/history?device_id=entrance-1&event_id=abc");
  assert.equal("fetch" in w.listeners, false);
});

test("malformed missing and null payloads still display a generic notification", async () => {
  for (const data of [null, { json: () => null }, { json: () => { throw Error("not JSON"); } }]) {
    const w = worker(); await w.fire("push", { data });
    assert.equal(w.shown[0].title, "おかえりんく");
    assert.equal(w.shown[0].options.data.url, "https://app.example/history");
  }
});

test("payloads cannot open external origins or arbitrary internal routes", async () => {
  for (const url of ["https://evil.example/history", "//evil.example/history", "/login", "javascript:alert(1)"]) {
    const w = worker(); await w.fire("push", { data: { json: () => ({ url }) } });
    assert.equal(w.shown[0].options.data.url, "https://app.example/history");
  }
});

test("click focuses and navigates an existing app window", async () => {
  const navigated = [], focused = [];
  const w = worker([{ url: "https://app.example/settings", navigate: async url => navigated.push(url), focus: async () => focused.push(true) }]);
  let closed = false;
  await w.fire("notificationclick", { notification: { data: { url: "https://app.example/history?event_id=abc" }, close() { closed = true; } } });
  assert.equal(closed, true);
  assert.deepEqual(navigated, ["https://app.example/history?event_id=abc"]);
  assert.equal(focused.length, 1); assert.equal(w.opened.length, 0);
});

test("a stale app window or foreign notification data falls back to opening safely", async () => {
  const w = worker([{ url: "https://app.example/settings", navigate: async () => { throw Error("closed"); }, focus: async () => {} }]);
  await w.fire("notificationclick", { notification: { data: { url: "https://evil.example/steal" }, close() {} } });
  assert.deepEqual(w.opened, ["https://app.example/history"]);
});
