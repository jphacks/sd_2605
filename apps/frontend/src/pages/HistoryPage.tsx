import { useEffect, useState } from "react";
import { History, RefreshCw, House, LogOut, Volume2 } from "lucide-react";
import { apiRequest, devicePath, DeviceEvent, eventLabel, formatDateTime } from "../api";

import { useDevices } from "../devices";

export default function HistoryPage() {
  const { active } = useDevices();
  const deviceName = active?.name ?? "";
  const [events, setEvents] = useState<DeviceEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const refresh = async () => {
    setLoading(true); setError("");
    try { setEvents(await apiRequest<DeviceEvent[]>(devicePath("/events"))); }
    catch { setError("履歴を取得できませんでした。時間をおいて再読み込みしてください。"); }
    finally { setLoading(false); }
  };
  useEffect(() => { void refresh(); }, []);
  return <div className="page-content">
    <div className="page-heading"><div><p className="eyebrow">YOUR HOME TIMELINE</p><h1>できごとの履歴</h1><p className="muted">{deviceName} の記録を確認できます。</p></div><button className="button button-outline" onClick={() => void refresh()} disabled={loading}><RefreshCw size={15} />更新</button></div>
    {error && <div className="inline-alert error">{error}</div>}
    <section className="activity-card">{events.length ? events.map(event => <div className="activity-row" key={event.event_id}><div className={`event-symbol ${event.event_type === "left_home" ? "event-left_home" : event.event_type === "arrived_home" ? "event-arrived_home" : ""}`}>{event.event_type === "left_home" ? <LogOut size={17} /> : event.event_type === "arrived_home" ? <House size={17} /> : <Volume2 size={17} />}</div><div className="activity-main"><strong>{eventLabel(event)}</strong><small>{formatDateTime(event.occurred_at)}</small></div><time>{new Date(event.occurred_at).toLocaleDateString("ja-JP", { year: "numeric", month: "numeric", day: "numeric" })}</time></div>) : <div className="empty-state"><div className="empty-icon"><History size={23} /></div><h2>{loading ? "履歴を読み込んでいます" : "履歴はまだありません"}</h2><p>外出・帰宅・音声再生などの記録がここに表示されます。</p></div>}</section>
  </div>;
}
