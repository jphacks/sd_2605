import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import ts from "typescript";

// Execute the real API/store modules with browser storage and HTTP responses mocked.
const storage = () => {
  const values = new Map();
  return { getItem: key => values.get(key) ?? null, setItem: (key, value) => values.set(key, value), removeItem: key => values.delete(key) };
};
globalThis.localStorage = storage();
globalThis.sessionStorage = storage();
globalThis.window = new EventTarget();
const moduleURL = code => `data:text/javascript;base64,${Buffer.from(code).toString("base64")}`;
const source = name => readFileSync(new URL(`../src/${name}.ts`, import.meta.url), "utf8");
const compile = code => ts.transpileModule(code.replace(/import\.meta\.env\.VITE_\w+/g, "undefined"), {
  compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 },
}).outputText;
const reactURL = moduleURL("export const useSyncExternalStore = () => {};");
const devicesURL = moduleURL(compile(source("devices")).replace('from "react"', `from "${reactURL}"`));
const devices = await import(devicesURL);
const apiURL = moduleURL(compile(source("api")).replace('from "./devices"', `from "${devicesURL}"`));
const api = await import(apiURL);

test("permission denial after login preserves the user session and device registration access", async () => {
  devices.clearAllSessions();
  const requests = [];
  globalThis.fetch = async (url, init) => {
    requests.push({ url, init });
    if (url.endsWith("/user_sessions")) return Response.json({ token: "valid-user", user: { id: 1, email: "owner@example.com" } });
    if (url.endsWith("/devices")) return Response.json({ device_id: "my-device" }, { status: 201 });
    return Response.json({ error: { code: "device_forbidden", message: "Permission denied" } }, { status: 403 });
  };
  await api.login("owner@example.com", "test-password");
  let expired = 0;
  const listener = () => expired++;
  window.addEventListener("tadaima:session-expired", listener);
  await assert.rejects(api.apiRequest(api.devicePath("/status")), error => error.status === 403);
  assert.equal(devices.getSession().token, "valid-user");
  assert.equal(expired, 0);
  await api.registerDevice("my-device", "device-password");
  assert.equal(devices.getSession("my-device").token, "valid-user");
  assert.equal(new Headers(requests.at(-1).init.headers).get("Authorization"), "Bearer valid-user");
  window.removeEventListener("tadaima:session-expired", listener);
});

test("an actual invalid login still clears its session on 401", async () => {
  devices.clearAllSessions();
  devices.saveSession({ token: "expired", device_id: "my-device" });
  globalThis.fetch = async () => Response.json({ error: { code: "invalid_credentials" } }, { status: 401 });
  await assert.rejects(api.apiRequest(api.devicePath("/status")), error => error.status === 401);
  assert.equal(devices.getSession("my-device"), null);
});

test("google login posts the credential and stores the session", async () => {
  devices.clearAllSessions();
  let body;
  globalThis.fetch = async (url, init) => {
    assert.equal(String(url).endsWith("/google_sessions"), true);
    assert.equal(init.method, "POST");
    body = JSON.parse(init.body);
    return Response.json({
      token: "google-token",
      expires_at: "2099-01-01T00:00:00Z",
      user: { id: 9, email: "person@example.com" },
    });
  };
  await api.loginWithGoogle("google-credential");
  assert.equal(body.credential, "google-credential");
  assert.equal(devices.getSession().token, "google-token");
});

test("google login shows the API error and does not store a session", async () => {
  devices.clearAllSessions();
  globalThis.fetch = async () => Response.json({
    error: { code: "invalid_google_token", message: "Googleログインに失敗しました。もう一度お試しください。" },
  }, { status: 401 });
  await assert.rejects(
    api.loginWithGoogle("bad"),
    error => error.message === "Googleログインに失敗しました。もう一度お試しください。",
  );
  assert.equal(devices.getSession(), null);
});

test("auth config reads the Google client id", async () => {
  globalThis.fetch = async (url, init) => {
    assert.equal(String(url).endsWith("/auth_config"), true);
    assert.equal(init, undefined);
    return Response.json({ google_client_id: "client-id" });
  };
  assert.deepEqual(await api.getAuthConfig(), { google_client_id: "client-id" });
});

test("a delayed 401 from the old login does not erase a new login", async () => {
  devices.clearAllSessions();
  devices.saveSession({ token: "old-token", device_id: "my-device" });
  let finish;
  globalThis.fetch = () => new Promise(resolve => { finish = resolve; });
  const pending = api.apiRequest(api.devicePath("/status"));
  devices.saveSession({ token: "new-token", device_id: "my-device" });
  finish(Response.json({ error: { code: "invalid_credentials" } }, { status: 401 }));
  await assert.rejects(pending, error => error.status === 401);
  assert.equal(devices.getSession("my-device").token, "new-token");
});
