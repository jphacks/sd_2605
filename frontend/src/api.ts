/** Web-facing Rails API. M5Stack uses its own unauthenticated device endpoints. */
import { clearSession, getActiveDeviceId, getSession, saveSession } from "./devices";
export { clearSession, getSession };
export const API_BASE_URL = (import.meta.env.VITE_API_BASE_URL || "/api").replace(/\/$/, "");
export type AudioMeta = { version: number; byte_size: number; content_type: "audio/wav"; path: string; file_name?: string; updated_at?: string | null; registered_by?: string | null };
export type DeviceStatus = {
  device_id: string;
  online: boolean;
  last_seen_at?: string | null;
  message_audio?: AudioMeta | null;
  leave_audio?: AudioMeta | null;
  arrive_audio?: AudioMeta | null;
  message_count?: number;
  message_updated_at?: string | null;
};
export type DeviceEvent = { event_id: string; event_type: "left_home" | "arrived_home" | "message_played" | string; occurred_at: string };
export type DeviceSettings = { volume: number };

export class ApiError extends Error {
  status: number;
  code?: string;
  constructor(status: number, message: string, code?: string) { super(message); this.status = status; this.code = code; }
}

type UserAuthResponse = {
  token: string;
  expires_at?: string;
  user: { id: number; email: string };
};

async function readApiError(response: Response, fallback: string): Promise<ApiError> {
  let message = fallback;
  let code: string | undefined;
  try {
    const body = await response.json();
    message = body?.error?.message || message;
    code = body?.error?.code;
  } catch { /* use fallback */ }
  return new ApiError(response.status, message, code);
}

async function saveUserSession(response: Response): Promise<void> {
  const data = await response.json() as UserAuthResponse;
  if (!data.token || !data.user?.id || !data.user.email) {
    throw new Error("認証サーバーから正しいユーザー情報を取得できませんでした。");
  }
  saveSession({
    token: data.token,
    device_id: getActiveDeviceId(),
    expires_at: data.expires_at,
  });
}

export async function register(email: string, password: string, passwordConfirmation: string): Promise<void> {
  if (password !== passwordConfirmation) throw new Error("確認用パスワードが一致しません。");

  const response = await fetch(`${API_BASE_URL}/users`, {
    method: "POST",
    headers: { "Content-Type": "application/json; charset=utf-8" },
    body: JSON.stringify({ email: email.trim(), password }),
  });
  if (!response.ok) throw await readApiError(response, `新規登録に失敗しました（HTTP ${response.status}）。`);
  await saveUserSession(response);
}

export async function getAuthConfig(): Promise<{ google_client_id: string | null }> {
  const response = await fetch(`${API_BASE_URL}/auth_config`);
  if (!response.ok) throw await readApiError(response, "ログイン設定を取得できませんでした。");
  const data = await response.json() as { google_client_id?: string | null };
  return { google_client_id: data.google_client_id || null };
}

export async function loginWithGoogle(credential: string): Promise<void> {
  const response = await fetch(`${API_BASE_URL}/google_sessions`, {
    method: "POST",
    headers: { "Content-Type": "application/json; charset=utf-8" },
    body: JSON.stringify({ credential }),
  });
  if (!response.ok) throw await readApiError(response, `Googleログインに失敗しました（HTTP ${response.status}）。`);
  await saveUserSession(response);
}

export async function login(email: string, password: string): Promise<void> {
  const response = await fetch(`${API_BASE_URL}/user_sessions`, {
    method: "POST",
    headers: { "Content-Type": "application/json; charset=utf-8" },
    body: JSON.stringify({ email: email.trim(), password }),
  });
  if (!response.ok) throw await readApiError(response, `ログインに失敗しました（HTTP ${response.status}）。`);
  await saveUserSession(response);
}

export async function loginDevice(deviceId: string, password: string): Promise<void> {
  const userSession = getSession();
  if (!userSession) throw new Error("デバイスを再ログインするには、先にアカウントへログインしてください。");

  const response = await fetch(`${API_BASE_URL}/sessions`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json; charset=utf-8",
      Authorization: `Bearer ${userSession.token}`,
    },
    body: JSON.stringify({ device_id: deviceId.trim(), password }),
  });
  if (!response.ok) {
    throw await readApiError(response, response.status === 401
      ? "デバイスIDまたはパスワードが正しくありません。"
      : `ログインに失敗しました（HTTP ${response.status}）。`);
  }
  const data = await response.json() as UserAuthResponse & { device_id: string };
  if (!data.token || !data.user?.id || !data.user.email || data.device_id !== deviceId.trim()) {
    throw new Error("認証サーバーから正しいデバイス情報を取得できませんでした。");
  }
  saveSession(data); // デバイスごとに保存し、このデバイスを操作中にする
}

export async function registerDevice(deviceId: string, password: string): Promise<void> {
  const userSession = getSession();
  if (!userSession) throw new Error("デバイスを登録するには、先にアカウントへログインしてください。");

  const id = deviceId.trim();
  const data = await apiRequest<{ device_id: string }>("/devices", {
    method: "POST",
    body: JSON.stringify({ device_id: id, password }),
  });
  if (data.device_id !== id) throw new Error("登録したデバイスIDを確認できませんでした。");
  saveSession({ ...userSession, device_id: id });
}

export async function apiRequest<T>(path: string, init: RequestInit = {}): Promise<T> {
  const headers = new Headers(init.headers);
  if (init.body && !(init.body instanceof FormData) && !(init.body instanceof Blob) && !headers.has("Content-Type")) headers.set("Content-Type", "application/json; charset=utf-8");
  // /devices/:id/... へのリクエストは、そのデバイスのセッションで認証する
  const target = /^\/devices\/([^/?#]+)/.exec(path);
  const deviceId = target ? decodeURIComponent(target[1]) : getActiveDeviceId();
  const session = getSession(deviceId);
  if (session) headers.set("Authorization", `Bearer ${session.token}`);
  const response = await fetch(`${API_BASE_URL}${path}`, { ...init, headers });
  if (response.status === 401 && session && getSession(deviceId)?.token === session.token) {
    clearSession(deviceId);
    window.dispatchEvent(new CustomEvent("tadaima:session-expired", { detail: { deviceId } }));
  }
  if (!response.ok) {
    let message = `HTTP ${response.status}`;
    let code: string | undefined;
    try { const body = await response.json(); message = body?.error?.message || message; code = body?.error?.code; } catch { /* retain HTTP status */ }
    throw new ApiError(response.status, message, code);
  }
  if (response.status === 204) return undefined as T;
  return response.json() as Promise<T>;
}
export const devicePath = (suffix: string, deviceId: string = getActiveDeviceId()) => `/devices/${encodeURIComponent(deviceId)}${suffix}`;
export const formatDateTime = (value?: string | null) => value
  ? new Date(value).toLocaleString("ja-JP", { year: "numeric", month: "numeric", day: "numeric", hour: "2-digit", minute: "2-digit" })
  : "—";
export const eventLabel = (event: DeviceEvent) => ({ left_home: "お出かけしました", arrived_home: "帰宅しました", message_played: "音声メッセージを再生しました" }[event.event_type] || "イベント");
