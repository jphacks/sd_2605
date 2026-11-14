import { useEffect, useState } from "react";
import { Bell, BellOff } from "lucide-react";
import { apiRequest } from "../api";
import { disablePush, enablePush, pushState, pushSupportMessage, testPush } from "../push";
import type { PushConfig } from "../push";

export default function PushNotificationSettings() {
  const [config, setConfig] = useState<PushConfig | null>(null);
  const [enabled, setEnabled] = useState(false);
  const [busy, setBusy] = useState(true);
  const [error, setError] = useState("");
  const [message, setMessage] = useState("");
  const unsupported = pushSupportMessage();
  useEffect(() => {
    let current = true;
    void (async () => {
      try {
        const settings = await apiRequest<PushConfig>("/push_config");
        const subscribed = await pushState(settings);
        if (current) { setConfig(settings); setEnabled(subscribed); }
      } catch { if (current) setError("通知設定を読み込めませんでした。ページを再読み込みしてください。"); }
      finally { if (current) setBusy(false); }
    })();
    return () => { current = false; };
  }, []);
  const toggle = async () => {
    if (!config) return;
    setBusy(true); setError(""); setMessage("");
    try {
      if (enabled) { await disablePush(); setEnabled(false); setMessage("このブラウザの通知を停止しました。"); }
      else { await enablePush(config); setEnabled(true); setMessage("このブラウザの通知を有効にしました。"); }
    } catch (error) { setError(error instanceof Error ? error.message : "通知の設定を変更できませんでした。"); }
    finally { setBusy(false); }
  };
  const sendTest = async () => {
    setBusy(true); setError(""); setMessage("");
    try { await testPush(); setMessage("テスト通知を送信しました。通知センターを確認してください。"); }
    catch (error) { setError(error instanceof Error ? error.message : "テスト通知を送信できませんでした。"); }
    finally { setBusy(false); }
  };
  return <section className="settings-card" aria-labelledby="push-settings-title">
    <div className="settings-card-heading"><div className="settings-icon settings-icon-blue"><Bell size={19} /></div><div><h2 id="push-settings-title">ブラウザ通知</h2><p>差し出し手として参加している家から、外出・帰宅や声の返信を受け取れます。</p></div></div>
    <p className="muted">この端末・ブラウザごとの設定です。メール通知は現在停止しています。</p>
    {unsupported && <div className="inline-alert info">{unsupported}</div>}
    {!unsupported && config && !config.eligible && <div className="inline-alert info">通知を受け取るには、家の招待コードで差し出し手として参加してください。</div>}
    {!unsupported && config && !config.configured && <div className="inline-alert info">通知機能は準備中です。時間をおいて試してください。</div>}
    {error && <div className="inline-alert error" role="alert">{error}</div>}
    {message && <div className="inline-alert success" role="status">{message}</div>}
    <div className="settings-footer"><span className="status-pill">{busy ? "確認中…" : enabled ? "このブラウザで有効" : "このブラウザでは無効"}</span>
      <div className="push-settings-actions"><button className="button button-outline" onClick={() => void toggle()} disabled={busy || Boolean(unsupported) || !config || (!enabled && (!config.configured || !config.eligible))}>{enabled ? <BellOff size={15} /> : <Bell size={15} />}{enabled ? "通知を停止" : "通知を有効にする"}</button>
      {enabled && <button className="button button-outline" onClick={() => void sendTest()} disabled={busy}>通知をテスト</button>}</div>
    </div>
  </section>;
}
