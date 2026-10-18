import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import ts from "typescript";

const storage = () => {
  const values = new Map();
  return { getItem: key => values.get(key) ?? null, setItem: (key, value) => values.set(key, value), removeItem: key => values.delete(key) };
};

async function loadDevices(sourceSuffix, setup) {
  globalThis.localStorage = storage();
  globalThis.sessionStorage = storage();
  setup();
  const moduleURL = code => `data:text/javascript;base64,${Buffer.from(code).toString("base64")}`;
  const source = readFileSync(new URL("../src/devices.ts", import.meta.url), "utf8") + sourceSuffix;
  const compiled = ts.transpileModule(source.replace(/import\.meta\.env\.VITE_\w+/g, "undefined"), {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 },
  }).outputText;
  const reactURL = moduleURL("export const useSyncExternalStore = () => {};");
  const devicesURL = moduleURL(compiled.replace('from "react"', `from "${reactURL}"`));
  return import(devicesURL);
}

test("the automatic entrance placeholder is removed and the account stays logged in", async () => {
  const devices = await loadDevices("\n// placeholder only", () => {
    localStorage.setItem("tadaima.devices", JSON.stringify([{ id: "entrance-1", name: "玄関" }]));
    localStorage.setItem("tadaima.activeDevice", "entrance-1");
    sessionStorage.setItem("tadaima.web.sessions", JSON.stringify({
      "entrance-1": { token: "user-token", device_id: "entrance-1" },
    }));
  });
  assert.deepEqual(JSON.parse(localStorage.getItem("tadaima.devices")), []);
  assert.equal(devices.getActiveDeviceId(), "");
  assert.equal(devices.getUserSession().token, "user-token");
  assert.equal(devices.getSession("entrance-1"), null);
  assert.equal(localStorage.getItem("tadaima.droppedLegacyPlaceholder"), "1");
});

test("a renamed entrance device and other real devices stay registered", async () => {
  const devices = await loadDevices("\n// keep real devices", () => {
    localStorage.setItem("tadaima.devices", JSON.stringify([
      { id: "entrance-1", name: "実家" },
      { id: "living-1", name: "居間" },
    ]));
    localStorage.setItem("tadaima.activeDevice", "living-1");
    sessionStorage.setItem("tadaima.web.sessions", JSON.stringify({
      "entrance-1": { token: "user-token", device_id: "entrance-1" },
      "living-1": { token: "user-token", device_id: "living-1" },
    }));
  });
  const saved = JSON.parse(localStorage.getItem("tadaima.devices"));
  assert.deepEqual(saved.map(device => device.id), ["entrance-1", "living-1"]);
  assert.equal(devices.getSession("entrance-1").token, "user-token");
  assert.equal(devices.getSession("living-1").token, "user-token");
  assert.equal(devices.getActiveDeviceId(), "living-1");
});
