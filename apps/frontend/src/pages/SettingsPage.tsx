import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { Settings2, Volume2, Save, RefreshCw, Wifi, ShieldCheck, Lock, Smile, ChevronRight } from "lucide-react";
import { apiRequest, devicePath, DeviceSettings, DeviceStatus, formatDateTime } from "../api";

import { useDevices } from "../devices";
import PushNotificationSettings from "../components/PushNotificationSettings";

export default function SettingsPage() {
  const { active, activeRole } = useDevices();
  // 設定を変えられるのは居住者だけ。差し出し手には変更の画面を出さない（役割が不明なときは居住者と同じ。保存が403なら差し出し手に切り替わる）
  const readOnly = activeRole === "messenger";
  const deviceName = active?.name ?? "";
  const deviceId = active?.id ?? "";
  const [settings, setSettings] = useState<DeviceSettings>({
    volume: 70,
    leave_greeting_enabled: true,
    arrive_greeting_enabled: true,
  });
  const [status, setStatus] = useState<DeviceStatus | null>(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState("");
  const [error, setError] = useState("");
  const refresh = async () => {
    setLoading(true); setError("");
    try {
      const [current, device] = await Promise.all([apiRequest<DeviceSettings>(devicePath("/settings")), apiRequest<DeviceStatus>(devicePath("/status"))]);
      setSettings(current); setStatus(device);
    } catch (e) { setError(e instanceof Error ? e.message : "デバイス情報を読み込めませんでした。"); }
    finally { setLoading(false); }
  };
  useEffect(() => { void refresh(); }, []);
  const save = async () => {
    setSaving(true); setMessage(""); setError("");
    try { const saved = await apiRequest<DeviceSettings>(devicePath("/settings"), { method: "PATCH", body: JSON.stringify(settings) }); setSettings(saved); setMessage("デバイス設定を保存しました。"); }
    catch (e) { setError(e instanceof Error ? e.message : "デバイス設定を保存できませんでした。"); }
    finally { setSaving(false); }
  };
  return <div className="page-content">
    <div className="page-heading"><div><p className="eyebrow">DEVICE INFORMATION</p><h1>デバイス情報</h1><p className="muted">{deviceName} の接続状態{readOnly ? "を確認します。" : "や再生音量を確認します。"}</p></div><div className="heading-icon"><Settings2 size={21} /></div></div>
    {error && <div className="inline-alert error">{error}</div>}{message && <div className="inline-alert success">{message}</div>}
    {readOnly && <div className="inline-alert info" role="status"><Lock size={13} /> あなたはこの家の差し出し手です。音声メッセージの登録はできますが、デバイスの設定を変更できるのは居住者だけです。</div>}
    <PushNotificationSettings />
    {!readOnly && <><section className="settings-card"><div className="settings-card-heading"><div className="settings-icon settings-icon-peach"><Volume2 size={19} /></div><div><h2>再生音量</h2><p>デバイスから流れる音声メッセージの音量</p></div><strong className="volume-value">{settings.volume}%</strong></div>
      <input className="volume-slider" type="range" min="0" max="100" step="5" value={settings.volume} disabled={loading} onChange={e => setSettings(s => ({ ...s, volume: Number(e.target.value) }))} /><div className="range-labels"><span>小さい</span><span>大きい</span></div>
    </section>
    <section className="settings-card greeting-settings">
      <div className="settings-card-heading"><div className="settings-icon settings-icon-blue"><ShieldCheck size={19} /></div><div><h2>挨拶の再生</h2><p>外出・帰宅時に登録した音声を再生するか選べます。</p></div></div>
      <label className="setting-row greeting-toggle">
        <span><strong>外出時の挨拶</strong><small>外出を検知したときに音声を再生します。</small></span>
        <input type="checkbox" checked={settings.leave_greeting_enabled} disabled={loading || saving} onChange={e => setSettings(s => ({ ...s, leave_greeting_enabled: e.target.checked }))} />
      </label>
      <label className="setting-row greeting-toggle">
        <span><strong>帰宅時の挨拶</strong><small>帰宅を検知したときに音声を再生します。</small></span>
        <input type="checkbox" checked={settings.arrive_greeting_enabled} disabled={loading || saving} onChange={e => setSettings(s => ({ ...s, arrive_greeting_enabled: e.target.checked }))} />
      </label>
    </section></>}
    <Link className="receive-link-card" to="/face"><span className="device-panel-icon"><Smile size={19} /></span><span><strong>ロボットの顔</strong><small>{readOnly ? "この家に保存された顔を見られます。" : "写真からパーツを選んで、この家の顔を保存できます。"}</small></span><ChevronRight size={18} /></Link>
    <section className="settings-card device-card"><div className="settings-card-heading"><div className="settings-icon settings-icon-blue"><Wifi size={19} /></div><div><h2>接続情報</h2><p>デバイスとの最終通信状況</p></div><span className="status-pill"><span className={`status-dot ${status?.online ? "" : "is-offline"}`} />{loading ? "確認中" : status?.online ? "オンライン" : "オフライン"}</span></div>
      <div className="device-info-row"><div><small>デバイスID</small><strong>{status?.device_id || deviceId}</strong></div><div><small>最終通信</small><strong>{formatDateTime(status?.last_seen_at)}</strong></div><div><small>外出の音声</small><strong>{status?.leave_audio ? "登録済み" : "未登録"}</strong></div><div><small>帰宅の音声</small><strong>{status?.arrive_audio || status?.message_audio ? "登録済み" : "未登録"}</strong></div></div>
      {!readOnly && <div className="setting-row"><div><strong>音声の再生</strong><small>設定変更はデバイスが次回設定を取得した際に反映されます。</small></div><ShieldCheck size={20} color="#6d8b70" /></div>}
      <div className="settings-footer"><span className="muted">情報を更新して最新の状態を確認できます。</span><button className="button button-outline" onClick={() => void refresh()} disabled={loading}><RefreshCw size={14} />更新</button></div>
    </section>
    {!readOnly && <div className="settings-footer"><span className="muted">変更はデバイスが次回設定を取得した際に反映されます。</span><button className="button button-primary" onClick={() => void save()} disabled={loading || saving}><Save size={16} />{saving ? "保存中…" : "設定を保存"}</button></div>}
  </div>;
}
