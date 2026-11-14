import { apiRequest } from "./api";

export type PushConfig = { configured: boolean; public_key: string | null; eligible: boolean };

export function pushSupportMessage(): string | null {
  const ios = /iPad|iPhone|iPod/.test(navigator.userAgent) || (navigator.platform === "MacIntel" && navigator.maxTouchPoints > 1);
  const standalone = window.matchMedia("(display-mode: standalone)").matches ||
    Boolean((navigator as Navigator & { standalone?: boolean }).standalone);
  if (ios && !standalone) return "iPhoneでは、Chromeなどの共有メニューから「ホーム画面に追加」し、追加したアイコンから開いてください（iOS 16.4以降）。";
  if (!window.isSecureContext || !("serviceWorker" in navigator) || !("PushManager" in window) || !("Notification" in window))
    return "この環境では通知を利用できません。対応する端末・ブラウザーで開いてください。";
  return null;
}

function applicationKey(value: string): Uint8Array {
  const padded = value.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(value.length / 4) * 4, "=");
  return Uint8Array.from(atob(padded), char => char.charCodeAt(0));
}

function sameKey(subscription: PushSubscription, key: string): boolean {
  const current = subscription.options.applicationServerKey;
  if (!current) return false;
  const expected = applicationKey(key), actual = new Uint8Array(current);
  return actual.length === expected.length && actual.every((value, index) => value === expected[index]);
}

async function browserSubscription(): Promise<PushSubscription | null> {
  if (!("serviceWorker" in navigator)) return null;
  const registration = await navigator.serviceWorker.getRegistration("/");
  return registration ? registration.pushManager.getSubscription() : null;
}

export async function pushState(config: PushConfig): Promise<boolean> {
  if (pushSupportMessage() || !config.public_key || Notification.permission !== "granted") return false;
  const subscription = await browserSubscription();
  if (!subscription || !sameKey(subscription, config.public_key)) return false;
  const state = await apiRequest<{ subscribed: boolean }>("/push_subscriptions/check", {
    method: "POST", body: JSON.stringify({ endpoint: subscription.endpoint })
  });
  return state.subscribed;
}

export async function enablePush(config: PushConfig): Promise<void> {
  const unsupported = pushSupportMessage();
  if (unsupported) throw new Error(unsupported);
  if (!config.eligible) throw new Error("通知を受け取るには、家の差し出し手として参加してください。");
  if (!config.configured || !config.public_key) throw new Error("通知機能は準備中です。時間をおいて試してください。");
  // Called directly by the settings button: iOS requires a user gesture.
  const permission = await Notification.requestPermission();
  if (permission !== "granted") throw new Error("端末またはブラウザーの設定で、このアプリの通知を許可してください。");
  const registration = await navigator.serviceWorker.register("/push-worker.js", { scope: "/", updateViaCache: "none" });
  await navigator.serviceWorker.ready;
  let subscription = await registration.pushManager.getSubscription();
  if (subscription && !sameKey(subscription, config.public_key)) {
    await apiRequest("/push_subscriptions", { method: "DELETE", body: JSON.stringify({ endpoint: subscription.endpoint }) });
    await subscription.unsubscribe(); subscription = null;
  }
  const created = !subscription;
  subscription ??= await registration.pushManager.subscribe({ userVisibleOnly: true, applicationServerKey: applicationKey(config.public_key) });
  try {
    await apiRequest("/push_subscriptions", { method: "POST", body: JSON.stringify({ subscription: subscription.toJSON() }) });
  } catch (error) {
    if (created) await subscription.unsubscribe();
    throw error;
  }
}

export async function disablePush(): Promise<void> {
  const subscription = await browserSubscription();
  if (!subscription) return;
  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), 5000);
  try {
    await apiRequest("/push_subscriptions", {
      method: "DELETE", body: JSON.stringify({ endpoint: subscription.endpoint }), signal: controller.signal
    });
  } finally {
    window.clearTimeout(timeout);
    await subscription.unsubscribe();
  }
}

export async function testPush(): Promise<void> {
  const subscription = await browserSubscription();
  if (!subscription) throw new Error("先にこのブラウザーの通知を有効にしてください。");
  await apiRequest("/push_subscriptions/test", { method: "POST", body: JSON.stringify({ endpoint: subscription.endpoint }) });
}
