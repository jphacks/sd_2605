/** Web-facing Rails API. M5Stack uses its own unauthenticated device endpoints. */
import { clearSession, getActiveDeviceId, getSession, saveSession } from "./devices";
export { clearSession, getSession };
export const API_BASE_URL = (import.meta.env.VITE_API_BASE_URL || "/api").replace(/\/$/, "");
export const DEVICE_ID = import.meta.env.VITE_DEVICE_ID || "entrance-1";
export type AudioMeta = { version: number; byte_size: number; content_type: "audio/wav"; path: string; file_name?: string; updated_at?: string | null; registered_by?: string | null };
export type DeviceStatus = {
  device_id: string;
  online: boolean;
  last_seen_at?: string | null;
  message_audio?: AudioMeta | null;
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

export async function login(deviceId: string, password: string): Promise<void> {
  const response = await fetch(`${API_BASE_URL}/sessions`, {
    method: "POST",
    headers: { "Content-Type": "application/json; charset=utf-8" },
    body: JSON.stringify({ device_id: deviceId.trim(), password }),
  });
  if (!response.ok) {
    let message = response.status === 401 ? "デバイスIDまたはパスワードが正しくありません。" : `ログインに失敗しました（HTTP ${response.status}）。`;
    try { const body = await response.json(); message = body?.error?.message || message; } catch { /* use default */ }
    throw new ApiError(response.status, message);
  }
  const data = await response.json() as { token: string; device_id: string; expires_at?: string };
  if (!data.token || data.device_id !== deviceId.trim()) throw new Error("認証サーバーから正しいセッション情報を取得できませんでした。");
  saveSession(data); // デバイスごとに保存し、このデバイスを操作中にする
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
  if (response.status === 401) { clearSession(deviceId); window.dispatchEvent(new CustomEvent("tadaima:session-expired", { detail: { deviceId } })); }
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
