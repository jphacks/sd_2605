import { useSyncExternalStore } from "react";

/**
 * 複数デバイスの管理。
 * - 一覧と表示名、操作中のデバイス … localStorage（秘密情報なし）
 * - アカウントのログイン … sessionStorage（デバイス一覧とは別）
 * - デバイスごとのログイン … sessionStorage（ブラウザを閉じると消える）
 */
export type StoredSession = { token: string; device_id: string; expires_at?: string };
export type SavedDevice = { id: string; name: string };
type Snapshot = { devices: SavedDevice[]; activeId: string; sessions: Record<string, StoredSession>; userSession: StoredSession | null };

export const DEVICE_ID_PATTERN = /^[A-Za-z0-9_-]{1,64}$/;
const DEVICES_KEY = "tadaima.devices";
const ACTIVE_KEY = "tadaima.activeDevice";
const SESSIONS_KEY = "tadaima.web.sessions";
const USER_KEY = "tadaima.web.user";
const LEGACY_SESSION_KEY = "tadaima.web.session"; // 単一デバイス版のセッション
const LEGACY_PLACEHOLDER_KEY = "tadaima.droppedLegacyPlaceholder";
const LEGACY_PLACEHOLDER_NAME = "玄関";

const isValid = (s?: StoredSession | null): s is StoredSession =>
  !!s && !!s.token && (!s.expires_at || new Date(s.expires_at).getTime() > Date.now());

function readJSON<T>(storage: Storage, key: string, fallback: T): T {
  try { const raw = storage.getItem(key); return raw ? (JSON.parse(raw) as T) : fallback; } catch { return fallback; }
}

function canOperate(s: Snapshot, id: string) {
  return !!s.sessions[id] || isValid(s.userSession);
}

/** 操作中のデバイスは、ログイン済みのものから選ぶ。未ログインのデバイスがあっても、ログイン済みが1台でもあればそちらを優先する */
function normalize(s: Snapshot): Snapshot {
  let activeId = s.activeId;
  const activeInList = s.devices.some(d => d.id === activeId);
  const operable = s.devices.some(d => canOperate(s, d.id));
  if (!activeInList || (operable && !canOperate(s, activeId))) {
    activeId = s.devices.find(d => canOperate(s, d.id))?.id ?? s.devices[0]?.id ?? "";
  }
  return { ...s, activeId };
}

function legacyPlaceholderIds(): Set<string> {
  const ids = new Set(["entrance-1"]);
  const configured = import.meta.env.VITE_DEVICE_ID;
  if (typeof configured === "string" && configured) ids.add(configured);
  return ids;
}

function writeStorage(s: Snapshot) {
  try {
    localStorage.setItem(DEVICES_KEY, JSON.stringify(s.devices));
    localStorage.setItem(ACTIVE_KEY, s.activeId);
    sessionStorage.setItem(SESSIONS_KEY, JSON.stringify(s.sessions));
    if (s.userSession) sessionStorage.setItem(USER_KEY, JSON.stringify(s.userSession));
    else sessionStorage.removeItem(USER_KEY);
  } catch { /* 保存できなくてもメモリ上では動作する */ }
}

function load(): Snapshot {
  const sessions: Record<string, StoredSession> = {};
  const saved = readJSON<Record<string, StoredSession>>(sessionStorage, SESSIONS_KEY, {});
  for (const [id, s] of Object.entries(saved)) if (isValid(s) && s.device_id === id) sessions[id] = s;
  const legacy = readJSON<StoredSession | null>(sessionStorage, LEGACY_SESSION_KEY, null);
  if (isValid(legacy) && !sessions[legacy.device_id]) sessions[legacy.device_id] = legacy;
  try { sessionStorage.removeItem(LEGACY_SESSION_KEY); } catch { /* noop */ }

  let userSession = readJSON<StoredSession | null>(sessionStorage, USER_KEY, null);
  if (!isValid(userSession)) userSession = null;
  if (!userSession) {
    const donor = Object.values(sessions).find(isValid);
    if (donor) userSession = { token: donor.token, device_id: "", expires_at: donor.expires_at };
  }

  let devices = readJSON<SavedDevice[]>(localStorage, DEVICES_KEY, [])
    .filter(d => d && typeof d.id === "string" && typeof d.name === "string");
  let droppedPlaceholder = false;
  try {
    if (!localStorage.getItem(LEGACY_PLACEHOLDER_KEY)) {
      const placeholderIds = legacyPlaceholderIds();
      const next = devices.filter(d => !(placeholderIds.has(d.id) && d.name === LEGACY_PLACEHOLDER_NAME));
      droppedPlaceholder = next.length !== devices.length;
      devices = next;
      if (droppedPlaceholder) {
        for (const id of placeholderIds) if (!devices.some(d => d.id === id)) delete sessions[id];
      }
      localStorage.setItem(LEGACY_PLACEHOLDER_KEY, "1");
    }
  } catch { /* 移行できなくても、この読み込みでは初期デバイスを足さない */ }
  const placeholderIds = legacyPlaceholderIds();
  for (const id of Object.keys(sessions)) {
    if (devices.some(d => d.id === id)) continue;
    if (placeholderIds.has(id)) {
      droppedPlaceholder = true;
      if (!userSession) userSession = { token: sessions[id].token, device_id: "", expires_at: sessions[id].expires_at };
      delete sessions[id];
      continue;
    }
    devices.push({ id, name: id });
  }
  let activeId = "";
  try { activeId = localStorage.getItem(ACTIVE_KEY) || ""; } catch { /* noop */ }
  const snapshot = normalize({ devices, activeId, sessions, userSession });
  if (droppedPlaceholder || userSession) writeStorage(snapshot);
  return snapshot;
}

let state: Snapshot = load();
const listeners = new Set<() => void>();

function commit(next: Snapshot) {
  state = normalize(next);
  writeStorage(state);
  listeners.forEach(l => l());
}

export const getActiveDeviceId = () => state.activeId;
export const getDeviceName = (id: string) => state.devices.find(d => d.id === id)?.name ?? id;

export function getUserSession(): StoredSession | null {
  const s = state.userSession;
  if (isValid(s)) return s;
  if (s) commit({ ...state, userSession: null });
  return null;
}

export function getSession(deviceId: string = state.activeId): StoredSession | null {
  if (!deviceId) return null;
  const s = state.sessions[deviceId];
  if (isValid(s)) return s;
  if (s) clearSession(deviceId);
  return null;
}

/** アカウントのログイン。デバイス一覧には追加しない */
export function saveUserAccount(session: { token: string; expires_at?: string }) {
  commit({ ...state, userSession: { token: session.token, device_id: "", expires_at: session.expires_at } });
}

/** ログイン成功時。未登録のデバイスは一覧に追加し、そのデバイスを操作中にする */
export function saveSession(session: StoredSession) {
  const id = session.device_id;
  if (!id) return;
  const devices = state.devices.some(d => d.id === id) ? state.devices : [...state.devices, { id, name: id }];
  commit({ ...state, devices, activeId: id, sessions: { ...state.sessions, [id]: session } });
}
export function clearSession(deviceId: string = state.activeId) {
  if (!deviceId || !state.sessions[deviceId]) return;
  const { [deviceId]: _removed, ...rest } = state.sessions;
  void _removed;
  commit({ ...state, sessions: rest });
}
export function clearAllSessions() { commit({ ...state, sessions: {}, userSession: null }); }

/** 操作するデバイスを切り替える。ログインしていないデバイスには切り替えられない */
export function setActiveDevice(id: string): boolean {
  if (!state.devices.some(d => d.id === id) || !canOperate(state, id)) return false;
  commit({ ...state, activeId: id });
  return true;
}
export function renameDevice(id: string, name: string) {
  commit({ ...state, devices: state.devices.map(d => d.id === id ? { ...d, name: name.trim() || d.id } : d) });
}
/** 一覧から外す（そのデバイスのログインも解除する）。アカウントのログインは維持する */
export function removeDevice(id: string) {
  const { [id]: _removed, ...sessions } = state.sessions;
  void _removed;
  commit({ ...state, devices: state.devices.filter(d => d.id !== id), activeId: state.activeId, sessions });
}

const subscribe = (l: () => void) => { listeners.add(l); return () => { listeners.delete(l); }; };

export function useDevices() {
  const s = useSyncExternalStore(subscribe, () => state);
  const active = s.devices.find(d => d.id === s.activeId) ?? null;
  return {
    devices: s.devices,
    activeId: s.activeId,
    active,
    authenticated: isValid(s.userSession) || Object.values(s.sessions).some(isValid),
    hasSession: (id: string) => !!s.sessions[id] || (isValid(s.userSession) && s.devices.some(d => d.id === id)),
  };
}
