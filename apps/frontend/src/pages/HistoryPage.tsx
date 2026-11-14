import { useEffect, useRef, useState } from "react";
import { useSearchParams } from "react-router-dom";
import { History, RefreshCw, House, LogOut, Volume2 } from "lucide-react";
import { apiRequest, devicePath, DeviceEvent, eventLabel, formatDateTime } from "../api";

import { DEVICE_ID_PATTERN, getDeviceName, setActiveDevice, useDevices } from "../devices";
import EventVoice from "../components/EventVoice";

export default function HistoryPage() {
  const { active, devices } = useDevices();
  const [params, setParams] = useSearchParams();
  const requestedDevice = params.get("device_id");
  const highlightedEvent = params.get("event_id");
  const deviceId = requestedDevice || active?.id || "";
  const deviceName = requestedDevice ? getDeviceName(requestedDevice) : active?.name ?? "";
  const [events, setEvents] = useState<DeviceEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const generation = useRef(0);
  useEffect(() => {
    if (requestedDevice && devices.some(device => device.id === requestedDevice) && setActiveDevice(requestedDevice)) {
      const next = new URLSearchParams(params); next.delete("device_id");
      setParams(next, { replace: true });
    }
  }, [requestedDevice, devices, params, setParams]);
  const refresh = async () => {
    const current = ++generation.current;
    setLoading(true); setError("");
    try {
      if (!DEVICE_ID_PATTERN.test(deviceId)) throw new Error("通知の家を確認できませんでした。");
      const found = await apiRequest<DeviceEvent[]>(devicePath("/events", deviceId));
      if (highlightedEvent && !found.some(event => event.event_id === highlightedEvent)) {
        const selected = await apiRequest<DeviceEvent>(devicePath(`/events/${encodeURIComponent(highlightedEvent)}`, deviceId));
        found.unshift(selected);
      }
      if (generation.current === current) setEvents(found);
    } catch (error) {
      if (generation.current === current) { setEvents([]); setError(error instanceof Error ? error.message : "履歴を取得できませんでした。"); }
    } finally { if (generation.current === current) setLoading(false); }
  };
  useEffect(() => {
    setEvents([]); void refresh();
    return () => { ++generation.current; };
  }, [deviceId, highlightedEvent]);
  useEffect(() => {
    if (!loading && highlightedEvent) document.getElementById(`event-${highlightedEvent}`)?.scrollIntoView({ block: "center", behavior: "smooth" });
  }, [loading, highlightedEvent, events]);
  return <div className="page-content">
    <div className="page-heading"><div><p className="eyebrow">YOUR HOME TIMELINE</p><h1>できごとの履歴</h1><p className="muted">{deviceName} の記録を確認できます。</p></div><button className="button button-outline" onClick={() => void refresh()} disabled={loading}><RefreshCw size={15} />更新</button></div>
    {error && <div className="inline-alert error">{error}</div>}
    <section className="activity-card">{events.length ? events.map(event => <div id={`event-${event.event_id}`} className={`activity-row${event.event_id === highlightedEvent ? " is-highlighted" : ""}`} key={`${deviceId}:${event.event_id}`}>
      <div className={`event-symbol ${event.event_type === "left_home" ? "event-left_home" : event.event_type === "arrived_home" ? "event-arrived_home" : ""}`}>{event.event_type === "left_home" ? <LogOut size={17} /> : event.event_type === "arrived_home" ? <House size={17} /> : <Volume2 size={17} />}</div>
      <div className="activity-main"><strong>{eventLabel(event)}</strong><small>{formatDateTime(event.occurred_at)}</small>{event.voice_available && <EventVoice deviceId={deviceId} eventId={event.event_id} />}</div>
      <time>{new Date(event.occurred_at).toLocaleDateString("ja-JP", { year: "numeric", month: "numeric", day: "numeric" })}</time>
    </div>) : <div className="empty-state"><div className="empty-icon"><History size={23} /></div><h2>{loading ? "履歴を読み込んでいます" : "履歴はまだありません"}</h2><p>外出・帰宅・音声再生などの記録がここに表示されます。</p></div>}</section>
  </div>;
}
