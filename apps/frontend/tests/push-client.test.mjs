import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import ts from "typescript";

const moduleURL = code => `data:text/javascript;base64,${Buffer.from(code).toString("base64")}`;
const apiURL = moduleURL("export const apiRequest = (...args) => globalThis.pushTestApi(...args);");
const source = readFileSync(new URL("../src/push.ts", import.meta.url), "utf8");
const compiled = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 } }).outputText;
const push = await import(moduleURL(compiled.replace('from "./api"', `from "${apiURL}"`)));
const key = Buffer.from(Uint8Array.from({ length: 65 }, (_, i) => i + 1)).toString("base64url");
const config = { configured: true, eligible: true, public_key: key };

function browser({ standalone = true, permission = "granted", subscribed = false } = {}) {
  const actions = [], requests = [];
  let current = null;
  const makeSubscription = () => ({
    endpoint: "https://web.push.apple.com/test-browser",
    options: { applicationServerKey: Uint8Array.from(Buffer.from(key, "base64url")).buffer },
    toJSON() { return { endpoint: this.endpoint, keys: { p256dh: key, auth: "browser-auth" } }; },
    async unsubscribe() { actions.push("unsubscribe"); current = null; return true; }
  });
  if (subscribed) current = makeSubscription();
  const registration = { pushManager: {
    getSubscription: async () => current,
    subscribe: async options => { assert.equal(options.userVisibleOnly, true); actions.push("subscribe"); current = makeSubscription(); return current; }
  } };
  const notification = { permission, requestPermission: async () => { actions.push("permission"); return permission; } };
  Object.defineProperty(globalThis, "navigator", { configurable: true, value: {
    userAgent: "Mozilla/5.0 (iPhone) CriOS/130", platform: "iPhone", maxTouchPoints: 5,
    serviceWorker: { getRegistration: async () => registration, ready: Promise.resolve(registration),
      register: async (url, options) => { actions.push("register"); assert.equal(url, "/push-worker.js"); assert.equal(options.updateViaCache, "none"); return registration; } }
  } });
  globalThis.window = { isSecureContext: true, matchMedia: () => ({ matches: standalone }),
    Notification: notification, PushManager: function() {}, setTimeout, clearTimeout };
  globalThis.Notification = notification;
  globalThis.pushTestApi = async (path, options) => { actions.push("api"); requests.push({ path, options }); return { subscribed: true, accepted: true }; };
  return { actions, requests, get subscription() { return current; } };
}

test("iPhone Chrome needs a Home Screen app and status checks never request permission", async () => {
  browser({ standalone: false }); assert.match(push.pushSupportMessage(), /ホーム画面に追加/);
  const b = browser({ subscribed: true });
  assert.equal(push.pushSupportMessage(), null);
  assert.equal(await push.pushState(config), true);
  assert.equal(b.actions.includes("permission"), false);
  assert.equal(b.requests[0].path, "/push_subscriptions/check");
});

test("explicit opt in asks permission first and saves the browser generated subscription", async () => {
  const b = browser(); await push.enablePush(config);
  assert.deepEqual(b.actions, ["permission", "register", "subscribe", "api"]);
  assert.equal(b.requests[0].path, "/push_subscriptions");
  assert.equal(JSON.parse(b.requests[0].options.body).subscription.endpoint, b.subscription.endpoint);
});

test("permission denial and ineligible accounts never register a subscription", async () => {
  let b = browser({ permission: "denied" });
  await assert.rejects(push.enablePush(config), /通知を許可/);
  assert.deepEqual(b.actions, ["permission"]);
  b = browser();
  await assert.rejects(push.enablePush({ ...config, eligible: false }), /差し出し手/);
  assert.deepEqual(b.actions, []);
});

test("failed server registration rolls back a newly created browser subscription", async () => {
  const b = browser(); globalThis.pushTestApi = async () => { throw Error("offline"); };
  await assert.rejects(push.enablePush(config), /offline/);
  assert.equal(b.subscription, null); assert.equal(b.actions.at(-1), "unsubscribe");
});

test("logout unsubscribe is attempted even when server deletion fails", async () => {
  const b = browser({ subscribed: true }); globalThis.pushTestApi = async () => { throw Error("offline"); };
  await assert.rejects(push.disablePush(), /offline/);
  assert.equal(b.subscription, null); assert.equal(b.actions.at(-1), "unsubscribe");
});

test("test notifications address only the current browser and denied permission is not enabled", async () => {
  const b = browser({ subscribed: true }); await push.testPush();
  assert.equal(b.requests[0].path, "/push_subscriptions/test");
  assert.equal(JSON.parse(b.requests[0].options.body).endpoint, b.subscription.endpoint);
  browser({ subscribed: true, permission: "denied" });
  assert.equal(await push.pushState(config), false);
});
