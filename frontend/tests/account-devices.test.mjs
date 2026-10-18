import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import ts from "typescript";

const storage = () => {
  const values = new Map();
  return { getItem: key => values.get(key) ?? null, setItem: (key, value) => values.set(key, value), removeItem: key => values.delete(key) };
};

async function loadDevices() {
  globalThis.localStorage = storage();
  globalThis.sessionStorage = storage();
  const moduleURL = code => `data:text/javascript;base64,${Buffer.from(code).toString("base64")}`;
  const source = readFileSync(new URL("../src/devices.ts", import.meta.url), "utf8");
  const compiled = ts.transpileModule(source.replace(/import\.meta\.env\.VITE_\w+/g, "undefined"), {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 },
  }).outputText;
  const reactURL = moduleURL("export const useSyncExternalStore = () => {};");
  const devicesURL = moduleURL(compiled.replace('from "react"', `from "${reactURL}"`));
  return import(devicesURL);
}

function savedIds() {
  return JSON.parse(localStorage.getItem("tadaima.devices")).map(device => device.id);
}

test("logging in with another account hides the previous account's devices", async () => {
  const devices = await loadDevices();
  devices.saveUserAccount({ token: "account-a", userId: 1 });
  devices.saveSession({ token: "account-a", device_id: "entrance-1" });
  devices.renameDevice("entrance-1", "玄関ロボット");

  devices.clearAllSessions();
  assert.deepEqual(savedIds(), []);
  assert.equal(devices.getActiveDeviceId(), "");

  devices.saveUserAccount({ token: "account-b", userId: 2 });
  assert.deepEqual(savedIds(), []);
  assert.equal(devices.getSession("entrance-1"), null);

  devices.saveSession({ token: "account-b", device_id: "atoms3r-cam-1" });
  devices.clearAllSessions();
  devices.saveUserAccount({ token: "account-a", userId: 1 });
  assert.deepEqual(savedIds(), ["entrance-1"]);
  assert.equal(devices.getDeviceName("entrance-1"), "玄関ロボット");
  assert.equal(devices.getActiveDeviceId(), "entrance-1");
  assert.equal(devices.getSession("atoms3r-cam-1"), null);
});
