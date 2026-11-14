import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import ts from "typescript";
const storage = () => {
  const values = new Map();
  return { getItem: k => values.get(k) ?? null, setItem: (k, v) => values.set(k, v), removeItem: k => values.delete(k) };
};
const url = s => `data:text/javascript;base64,${Buffer.from(s).toString("base64")}`;
let revision = 0;
async function reload() {
  const source = readFileSync(new URL("../src/devices.ts", import.meta.url), "utf8");
  const code = ts.transpileModule(source.replace(/import\.meta\.env\.VITE_\w+/g, "undefined"), {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 },
  }).outputText.replace('from "react"', `from "${url("export const useSyncExternalStore = () => {};")}"`);
  return import(url(code + `\n// ${revision++}`));
}
test("migrated login survives browser restart; logout and expiry still apply", async () => {
  globalThis.localStorage = storage();
  globalThis.sessionStorage = storage();
  const session = { token: "user-token", device_id: "", user_id: "42", expires_at: "2099-01-01T00:00:00Z" };
  sessionStorage.setItem("tadaima.web.user", JSON.stringify(session));
  let devices = await reload();
  assert.equal(devices.getUserSession().token, session.token);
  assert.equal(sessionStorage.getItem("tadaima.web.user"), null);
  devices.saveSession({ ...session, device_id: "front-door" });
  globalThis.sessionStorage = storage();
  devices = await reload();
  assert.equal(devices.getUserSession().token, session.token);
  assert.equal(devices.getSession("front-door").token, session.token);
  devices.clearAllSessions();
  devices = await reload();
  assert.equal(devices.getUserSession(), null);
  assert.equal(devices.getSession("front-door"), null);
  assert.equal(localStorage.getItem("tadaima.web.user"), null);
  localStorage.setItem("tadaima.web.user", JSON.stringify({ ...session, expires_at: "2000-01-01T00:00:00Z" }));
  devices = await reload();
  assert.equal(devices.getUserSession(), null);
});
