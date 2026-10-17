import { useSyncExternalStore } from "react";

/**
 * 複数デバイスの管理。
 * - 一覧と表示名、操作中のデバイス … localStorage（秘密情報なし）
 * - ログインセッション（トークン） … sessionStorage（デバイスごと。ブラウザを閉じると消える）
 */
export type StoredSession = { token: string; device_id: string; expires_at?: string };
export type SavedDevice = { id: string; name: string };
type Snapshot = { devices: SavedDevice[]; activeId: string; sessions: Record<string, StoredSession> };

export const DEVICE_ID_PATTERN = /^[A-Za-z0-9_-]{1,64}$/;
const DEVICES_KEY = "tadaima.devices";
const ACTIVE_KEY = "tadaima.activeDevice";
const SESSIONS_KEY = "tadaima.web.sessions";
const LEGACY_SESSION_KEY = "tadaima.web.session"; // 単一デバイス版のセッション
const DEFAULT_ID: string = import.meta.env.VITE_DEVICE_ID || "entrance-1";

const isValid = (s?: StoredSession | null): s is StoredSession =>
  !!s && !!s.token && (!s.expires_at || new Date(s.expires_at).getTime() > Date.now());

function readJSON<T>(storage: Storage, key: string, fallback: T): T {
  try { const raw = storage.getItem(key); return raw ? (JSON.parse(raw) as T) : fallback; } catch { return fallback; }
}

/** 操作中のデバイスは、ログイン済みのデバイスから選ぶ（1台でもログインしていれば未ログインのデバイスは選べない） */
function normalize(s: Snapshot): Snapshot {
  const ids = Object.keys(s.sessions);
  let activeId = s.activeId;
  if (!s.devices.some(d => d.id === activeId) || (ids.length && !s.sessions[activeId])) {
    activeId = s.devices.find(d => s.sessions[d.id])?.id ?? s.devices[0].id;
  }
  return { ...s, activeId };
}

function load(): Snapshot {
  const sessions: Record<string, StoredSession> = {};
  const saved = readJSON<Record<string, StoredSession>>(sessionStorage, SESSIONS_KEY, {});
  for (const [id, s] of Object.entries(saved)) if (isValid(s) && s.device_id === id) sessions[id] = s;
  const legacy = readJSON<StoredSession | null>(sessionStorage, LEGACY_SESSION_KEY, null);
  if (isValid(legacy) && !sessions[legacy.device_id]) sessions[legacy.device_id] = legacy;
  try { sessionStorage.removeItem(LEGACY_SESSION_KEY); } catch { /* noop */ }

  const devices = readJSON<SavedDevice[]>(localStorage, DEVICES_KEY, [])
    .filter(d => d && typeof d.id === "string" && typeof d.name === "string");
  for (const id of Object.keys(sessions)) if (!devices.some(d => d.id === id)) devices.push({ id, name: id });
  if (!devices.length) devices.push({ id: DEFAULT_ID, name: "玄関" });
  let activeId = "";
  try { activeId = localStorage.getItem(ACTIVE_KEY) || ""; } catch { /* noop */ }
  return normalize({ devices, activeId, sessions });
}

let state: Snapshot = load();
const listeners = new Set<() => void>();

function commit(next: Snapshot) {
  state = normalize(next);
  try {
    localStorage.setItem(DEVICES_KEY, JSON.stringify(state.devices));
    localStorage.setItem(ACTIVE_KEY, state.activeId);
    sessionStorage.setItem(SESSIONS_KEY, JSON.stringify(state.sessions));
  } catch { /* 保存できなくてもメモリ上では動作する */ }
  listeners.forEach(l => l());
}

export const getActiveDeviceId = () => state.activeId;
export const getDeviceName = (id: string) => state.devices.find(d => d.id === id)?.name ?? id;

export function getSession(deviceId: string = state.activeId): StoredSession | null {
  const s = state.sessions[deviceId];
  if (isValid(s)) return s;
  if (s) clearSession(deviceId);
  return null;
}

/** ログイン成功時。未登録のデバイスは一覧に追加し、そのデバイスを操作中にする */
export function saveSession(session: StoredSession) {
  const id = session.device_id;
  const devices = state.devices.some(d => d.id === id) ? state.devices : [...state.devices, { id, name: id }];
  commit({ devices, activeId: id, sessions: { ...state.sessions, [id]: session } });
}
export function clearSession(deviceId: string = state.activeId) {
  if (!state.sessions[deviceId]) return;
  const { [deviceId]: _removed, ...rest } = state.sessions;
  void _removed;
  commit({ ...state, sessions: rest });
}
export function clearAllSessions() { commit({ ...state, sessions: {} }); }

/** 操作するデバイスを切り替える。ログインしていないデバイスには切り替えられない */
export function setActiveDevice(id: string): boolean {
  if (!state.sessions[id]) return false;
  commit({ ...state, activeId: id });
  return true;
}
export function renameDevice(id: string, name: string) {
  commit({ ...state, devices: state.devices.map(d => d.id === id ? { ...d, name: name.trim() || d.id } : d) });
}
/** 一覧から外す（そのデバイスのログインも解除する）。最後の1台は外せない */
export function removeDevice(id: string) {
  if (state.devices.length <= 1) return;
  const { [id]: _removed, ...sessions } = state.sessions;
  void _removed;
  commit({ devices: state.devices.filter(d => d.id !== id), activeId: state.activeId, sessions });
}

const subscribe = (l: () => void) => { listeners.add(l); return () => { listeners.delete(l); }; };

export function useDevices() {
  const s = useSyncExternalStore(subscribe, () => state);
  const active = s.devices.find(d => d.id === s.activeId) ?? s.devices[0];
  return {
    devices: s.devices,
    activeId: s.activeId,
    active,
    authenticated: Object.keys(s.sessions).length > 0,
    hasSession: (id: string) => !!s.sessions[id],
  };
}
