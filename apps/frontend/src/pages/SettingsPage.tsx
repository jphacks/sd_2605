import { useEffect, useState } from "react";
import { Settings2, Volume2, Save, RefreshCw, Wifi, ShieldCheck } from "lucide-react";
import { apiRequest, devicePath, DeviceSettings, DeviceStatus, formatDateTime } from "../api";

import { useDevices } from "../devices";

export default function SettingsPage() {
  const { active } = useDevices();
  const [settings, setSettings] = useState<DeviceSettings>({ volume: 70 });
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
    try { const saved = await apiRequest<DeviceSettings>(devicePath("/settings"), { method: "PATCH", body: JSON.stringify(settings) }); setSettings(saved); setMessage("音量を保存しました。"); }
    catch (e) { setError(e instanceof Error ? e.message : "音量を保存できませんでした。"); }
    finally { setSaving(false); }
  };
  return <div className="page-content">
    <div className="page-heading"><div><p className="eyebrow">DEVICE INFORMATION</p><h1>デバイス情報</h1><p className="muted">{active.name} の接続状態や再生音量を確認します。</p></div><div className="heading-icon"><Settings2 size={21} /></div></div>
    {error && <div className="inline-alert error">{error}</div>}{message && <div className="inline-alert success">{message}</div>}
    <section className="settings-card"><div className="settings-card-heading"><div className="settings-icon settings-icon-peach"><Volume2 size={19} /></div><div><h2>再生音量</h2><p>デバイスから流れる音声メッセージの音量</p></div><strong className="volume-value">{settings.volume}%</strong></div>
      <input className="volume-slider" type="range" min="0" max="100" step="5" value={settings.volume} disabled={loading} onChange={e => setSettings(s => ({ ...s, volume: Number(e.target.value) }))} /><div className="range-labels"><span>小さい</span><span>大きい</span></div>
    </section>
    <section className="settings-card device-card"><div className="settings-card-heading"><div className="settings-icon settings-icon-blue"><Wifi size={19} /></div><div><h2>接続情報</h2><p>デバイスとの最終通信状況</p></div><span className="status-pill"><span className={`status-dot ${status?.online ? "" : "is-offline"}`} />{loading ? "確認中" : status?.online ? "オンライン" : "オフライン"}</span></div>
      <div className="device-info-row"><div><small>デバイスID</small><strong>{status?.device_id || active.id}</strong></div><div><small>最終通信</small><strong>{formatDateTime(status?.last_seen_at)}</strong></div><div><small>外出の音声</small><strong>{status?.leave_audio ? "登録済み" : "未登録"}</strong></div><div><small>帰宅の音声</small><strong>{status?.arrive_audio || status?.message_audio ? "登録済み" : "未登録"}</strong></div></div>
      <div className="setting-row"><div><strong>音声の再生</strong><small>挨拶やメッセージの再生タイミングはデバイス側で管理します。</small></div><ShieldCheck size={20} color="#6d8b70" /></div>
      <div className="settings-footer"><span className="muted">情報を更新して最新の状態を確認できます。</span><button className="button button-outline" onClick={() => void refresh()} disabled={loading}><RefreshCw size={14} />更新</button></div>
    </section>
    <div className="settings-footer"><span className="muted">音量の変更はデバイスが次回設定を取得した際に反映されます。</span><button className="button button-primary" onClick={() => void save()} disabled={loading || saving}><Save size={16} />{saving ? "保存中…" : "音量を保存"}</button></div>
  </div>;
}
