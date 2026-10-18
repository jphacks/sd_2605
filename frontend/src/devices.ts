import { useSyncExternalStore } from "react";

/**
 * 複数デバイスの管理。
 * - 一覧と表示名、操作中のデバイス … アカウントごとの localStorage（秘密情報なし。別のブラウザには引き継がない）
 * - アカウントのログイン … sessionStorage（デバイス一覧とは別）
 * - デバイスごとのログイン … そのアカウントの一覧と一緒に保存する
 */
export type StoredSession = { token: string; device_id: string; expires_at?: string; user_id?: string };
export type SavedDevice = { id: string; name: string };
type AccountRecord = { devices: SavedDevice[]; activeId: string; sessions: Record<string, StoredSession> };
type Snapshot = { accountId: string | null; devices: SavedDevice[]; activeId: string; sessions: Record<string, StoredSession>; userSession: StoredSession | null };

export const DEVICE_ID_PATTERN = /^[A-Za-z0-9_-]{1,64}$/;
const DEVICES_KEY = "tadaima.devices";
const ACCOUNTS_KEY = "tadaima.accounts";
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

function canOperate(s: Pick<Snapshot, "sessions" | "userSession">, id: string) {
  return !!s.sessions[id] || isValid(s.userSession);
}

/** 操作中のデバイスは、ログイン済みのものから選ぶ。未ログインのデバイスがあっても、ログイン済みが1台でもあればそちらを優先する */
function normalize<T extends Omit<Snapshot, "accountId">>(s: T): T {
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

function accountIdFromToken(token: string): string | null {
  const part = token.split(".")[1];
  if (!part) return null;
  try {
    const json = JSON.parse(atob(part.replace(/-/g, "+").replace(/_/g, "/")));
    if (typeof json.sub === "number" || typeof json.sub === "string") return String(json.sub);
  } catch { /* アカウントIDを持たないトークンはアカウント分けに使わない */ }
  return null;
}

function accountIdOf(session: StoredSession | null): string | null {
  if (!session) return null;
  if (session.user_id) return session.user_id;
  return accountIdFromToken(session.token);
}

function readAccounts(): Record<string, AccountRecord> {
  const saved = readJSON<Record<string, AccountRecord>>(localStorage, ACCOUNTS_KEY, {});
  const accounts: Record<string, AccountRecord> = {};
  for (const [id, record] of Object.entries(saved)) {
    if (!record || !Array.isArray(record.devices)) continue;
    accounts[id] = {
      devices: record.devices.filter(d => d && typeof d.id === "string" && typeof d.name === "string"),
      activeId: typeof record.activeId === "string" ? record.activeId : "",
      sessions: record.sessions && typeof record.sessions === "object" ? record.sessions : {},
    };
  }
  return accounts;
}

function writeAccount(accounts: Record<string, AccountRecord>, id: string, record: AccountRecord) {
  accounts[id] = record;
  localStorage.setItem(ACCOUNTS_KEY, JSON.stringify(accounts));
}

function writeStorage(s: Snapshot) {
  try {
    if (s.accountId) {
      writeAccount(readAccounts(), s.accountId, { devices: s.devices, activeId: s.activeId, sessions: s.sessions });
    }
    localStorage.setItem(DEVICES_KEY, JSON.stringify(s.devices));
    localStorage.setItem(ACTIVE_KEY, s.activeId);
    sessionStorage.setItem(SESSIONS_KEY, JSON.stringify(s.sessions));
    if (s.userSession) sessionStorage.setItem(USER_KEY, JSON.stringify(s.userSession));
    else sessionStorage.removeItem(USER_KEY);
  } catch { /* 保存できなくてもメモリ上では動作する */ }
}

function loadUnscoped(): Omit<Snapshot, "accountId"> {
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
  return normalize({ devices, activeId, sessions, userSession });
}

function load(): Snapshot {
  const base = loadUnscoped();
  const accountId = accountIdOf(base.userSession);
  if (!accountId) {
    const snapshot = { ...base, accountId: null };
    if (base.userSession) writeStorage(snapshot);
    return snapshot;
  }
  const accounts = readAccounts();
  const saved = accounts[accountId];
  // まだアカウント別の保存がない最初の読み込みだけ、今ログイン中のアカウントへ従来の一覧を移す。
  if (!saved && Object.keys(accounts).length === 0) {
    const snapshot = { ...base, accountId };
    writeStorage(snapshot);
    return snapshot;
  }
  const record = saved ?? { devices: [], activeId: "", sessions: {} };
  const snapshot = normalize({
    accountId,
    devices: record.devices,
    activeId: record.activeId,
    sessions: record.sessions,
    userSession: base.userSession,
  });
  writeStorage(snapshot);
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

/** アカウントのログイン。別のアカウントのデバイス一覧には切り替え、今の一覧には追加しない */
export function saveUserAccount(session: { token: string; expires_at?: string; userId?: number | string }) {
  const nextId = session.userId != null && String(session.userId) !== "" ? String(session.userId) : accountIdFromToken(session.token);
  const userSession: StoredSession = {
    token: session.token,
    device_id: "",
    expires_at: session.expires_at,
    ...(nextId ? { user_id: nextId } : {}),
  };
  if (!nextId || nextId === state.accountId) {
    commit({ ...state, accountId: nextId ?? state.accountId, userSession });
    return;
  }
  const accounts = readAccounts();
  if (state.accountId) writeAccount(accounts, state.accountId, { devices: state.devices, activeId: state.activeId, sessions: state.sessions });
  const saved = accounts[nextId] ?? { devices: [], activeId: "", sessions: {} };
  commit({ accountId: nextId, devices: saved.devices, activeId: saved.activeId, sessions: saved.sessions, userSession });
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
export function clearAllSessions() {
  if (state.accountId) {
    writeAccount(readAccounts(), state.accountId, { devices: state.devices, activeId: state.activeId, sessions: {} });
  }
  commit({ ...state, accountId: null, devices: [], activeId: "", sessions: {}, userSession: null });
}

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
