import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import ts from "typescript";

// 役割（居住者／差し出し手）と家のアイコンの保存を、実際のモジュールで確かめる。
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
const api = await import(moduleURL(compile(source("api")).replace('from "./devices"', `from "${devicesURL}"`)));

const saved = id => JSON.parse(localStorage.getItem("tadaima.devices")).find(d => d.id === id);
const signIn = (userId = 1) => { devices.clearAllSessions(); devices.saveUserAccount({ token: `user-${userId}`, userId }); };

test("accepting an invitation records the messenger role", async () => {
  signIn(10);
  globalThis.fetch = async () => Response.json({ device_id: "friend-home", role: "messenger" });
  await api.acceptDeviceInvitation("ABCD2345");
  assert.equal(saved("friend-home").role, "messenger");
});

test("registering a new device makes the user a resident; joining an existing one stays unknown", async () => {
  signIn(11);
  globalThis.fetch = async () => Response.json({ device_id: "my-home" }, { status: 201 });
  await api.registerDevice("my-home", "device-password");
  assert.equal(saved("my-home").role, "resident");

  globalThis.fetch = async () => Response.json({ device_id: "old-home" }, { status: 200 });
  await api.registerDevice("old-home", "device-password");
  assert.equal(saved("old-home").role, undefined);
});

test("a role in the server response wins over the guess", async () => {
  signIn(12);
  globalThis.fetch = async () => Response.json({ device_id: "old-home", role: "messenger" }, { status: 200 });
  await api.registerDevice("old-home", "device-password");
  assert.equal(saved("old-home").role, "messenger");

  globalThis.fetch = async () => Response.json({ device_id: "old-home", online: true, role: "resident" });
  await api.apiRequest(api.devicePath("/status", "old-home"));
  assert.equal(saved("old-home").role, "resident");
});

test("an unknown role value is ignored", async () => {
  signIn(13);
  globalThis.fetch = async () => Response.json({ device_id: "odd-home", role: "admin" }, { status: 200 });
  await api.registerDevice("odd-home", "device-password");
  assert.equal(saved("odd-home").role, undefined);
});

test("resident_only on a device request corrects the role to messenger", async () => {
  signIn(14);
  globalThis.fetch = async () => Response.json({ device_id: "legacy-home" }, { status: 201 });
  await api.registerDevice("legacy-home", "device-password");
  assert.equal(saved("legacy-home").role, "resident");

  globalThis.fetch = async () => Response.json({ error: { code: "resident_only", message: "居住者だけです。" } }, { status: 403 });
  await assert.rejects(api.createDeviceInvitation("legacy-home"), error => error.status === 403 && error.code === "resident_only");
  assert.equal(saved("legacy-home").role, "messenger");
  // セッションは消えない
  assert.equal(devices.getUserSession().token, "user-14");
});

test("other 403 errors do not change the role", async () => {
  signIn(15);
  globalThis.fetch = async () => Response.json({ device_id: "keep-home" }, { status: 201 });
  await api.registerDevice("keep-home", "device-password");
  globalThis.fetch = async () => Response.json({ error: { code: "device_forbidden" } }, { status: 403 });
  await assert.rejects(api.apiRequest(api.devicePath("/status", "keep-home")), error => error.status === 403);
  assert.equal(saved("keep-home").role, "resident");
});

test("device icons persist per account and can be reset", () => {
  signIn(20);
  devices.saveSession({ token: "user-20", device_id: "icon-home" });
  devices.setDeviceIcon("icon-home", { kind: "preset", id: "cat" });
  assert.deepEqual(saved("icon-home").icon, { kind: "preset", id: "cat" });
  devices.setDeviceIcon("icon-home", { kind: "photo", dataUrl: "data:image/jpeg;base64,AAAA" });
  assert.equal(saved("icon-home").icon.kind, "photo");

  // 別アカウントには見えず、戻ってくると残っている
  devices.clearAllSessions();
  devices.saveUserAccount({ token: "user-21", userId: 21 });
  assert.equal(saved("icon-home"), undefined);
  devices.clearAllSessions();
  devices.saveUserAccount({ token: "user-20", userId: 20 });
  assert.equal(saved("icon-home").icon.kind, "photo");

  devices.setDeviceIcon("icon-home", null);
  assert.equal("icon" in saved("icon-home"), false);
});

test("renaming or re-selecting a device keeps its role and icon", () => {
  signIn(30);
  devices.saveSession({ token: "user-30", device_id: "keep" }, "messenger");
  devices.setDeviceIcon("keep", { kind: "preset", id: "star" });
  devices.renameDevice("keep", "実家");
  devices.saveSession({ token: "user-30", device_id: "keep" }); // 役割なしで再ログイン
  assert.equal(saved("keep").role, "messenger");
  assert.deepEqual(saved("keep").icon, { kind: "preset", id: "star" });
  assert.equal(saved("keep").name, "実家");
});
