import { useEffect, useState } from "react";
import { Wifi, Mic, ChevronRight, MessageCircle, RefreshCw, House, LogOut, CheckCircle2, Clock3 } from "lucide-react";
import { Link } from "react-router-dom";
import Robot from "../components/Robot";
import { apiRequest, devicePath, AudioMeta, DeviceStatus, DeviceEvent, eventLabel, formatDateTime } from "../api";
import type { RobotMode } from "../types";
import { useDevices } from "../devices";

export default function HomePage() {
  const { active } = useDevices();
  const deviceName = active?.name ?? "";
  const [status, setStatus] = useState<DeviceStatus | null>(null);
  const [events, setEvents] = useState<DeviceEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const today = new Date().toLocaleDateString("ja-JP", { month: "long", day: "numeric", weekday: "long" });
  const refresh = async () => {
    setLoading(true); setError("");
    try {
      const [device, timeline] = await Promise.all([
        apiRequest<DeviceStatus>(devicePath("/status")),
        apiRequest<DeviceEvent[]>(devicePath("/events?limit=5")),
      ]);
      setStatus(device); setEvents(timeline);
    } catch { setError("情報を取得できませんでした。時間をおいて再読み込みしてください。"); }
    finally { setLoading(false); }
  };
  useEffect(() => { void refresh(); }, []);
  const online = !!status?.online;
  const leaveAudio = status?.leave_audio;
  const arriveAudio = status?.arrive_audio ?? status?.message_audio;
  const audioRegistered = (audio?: AudioMeta | null) => !!audio && audio.byte_size > 0;
  const audioDetail = (audio: AudioMeta | null | undefined, empty: string) => audioRegistered(audio) ? `${audio?.registered_by || "登録済み"} ・ ${formatDateTime(audio?.updated_at)}` : empty;
  const mode: RobotMode = online ? "idle" : "offline";
  return <div className="page-content">
    <div className="page-heading"><div><p className="eyebrow">{today} ・ {deviceName}</p><h1>おかえりなさい。</h1><p className="muted">大切な人の声が、あなたの帰りを待っています。</p></div><div className="status-pill"><span className={`status-dot ${online ? "" : "is-offline"}`} />{loading ? "状態を確認中" : online ? "オンライン" : "オフライン"}</div></div>
    <section className="hero-card"><div className="hero-copy"><span className="eyebrow">A LITTLE MESSAGE FOR YOU</span><h2>声でつながる、いつもの場所。</h2><p>家族や友達から届いたメッセージを、外出と帰宅のときにロボットがお届けします。</p><div className="hero-actions"><Link className="button button-primary" to="/voice"><Mic size={17} />音声メッセージを登録 <ChevronRight size={16} /></Link><button className="button button-quiet" onClick={() => void refresh()} disabled={loading}><RefreshCw size={15} />更新</button></div></div><div className="hero-robot-wrap"><div className="hero-halo" /><Robot mode={mode} /><div className="robot-label"><span className={`status-dot ${online ? "" : "is-offline"}`} />{online ? "メッセージを届ける準備OK" : "接続を確認してください"}</div></div></section>
    {error && <div className="inline-alert error">{error}</div>}
    <div className="section-heading"><div><h2>おうちの状態</h2><p className="muted">音声の登録状況とデバイス情報</p></div></div>
    <div className="stats-grid">
      <article className="stat-card"><div className="stat-icon stat-icon-green"><Wifi size={20} /></div><div><span>デバイス接続</span><strong>{loading ? "確認中…" : online ? "オンライン" : "オフライン"}</strong><small>最終通信 {formatDateTime(status?.last_seen_at)}</small></div></article>
      <article className="stat-card"><div className="stat-icon stat-icon-green"><LogOut size={20} /></div><div><span>外出の音声</span><strong>{loading ? "確認中…" : audioRegistered(leaveAudio) ? "登録済み" : "未登録"}</strong><small>{loading ? "確認しています" : audioDetail(leaveAudio, "いってらっしゃいを登録できます")}</small></div><div className="message-status-icon">{audioRegistered(leaveAudio) ? <CheckCircle2 size={18} /> : <Clock3 size={18} />}</div></article>
      <article className="stat-card"><div className="stat-icon stat-icon-peach"><House size={20} /></div><div><span>帰宅の音声</span><strong>{loading ? "確認中…" : audioRegistered(arriveAudio) ? "登録済み" : "未登録"}</strong><small>{loading ? "確認しています" : audioDetail(arriveAudio, "おかえりを登録できます")}</small></div><div className="message-status-icon">{audioRegistered(arriveAudio) ? <CheckCircle2 size={18} /> : <Clock3 size={18} />}</div></article>
      <article className="stat-card"><div className="stat-icon stat-icon-lilac"><Mic size={20} /></div><div><span>メッセージ管理</span><strong>登録・差し替え</strong><small>音声の再生はデバイスで行います</small></div><Link to="/voice" className="card-arrow" aria-label="メッセージ登録へ"><ChevronRight size={18} /></Link></article>
    </div>
    <div className="section-heading"><div><h2>最近のできごと</h2><p className="muted">ロボットの記録</p></div><Link to="/history" className="text-button">履歴を見る <ChevronRight size={16} /></Link></div>
    <section className="activity-card">{events.length ? events.map(event => <div className="activity-row" key={event.event_id}><div className={`event-symbol ${event.event_type === "left_home" ? "event-left_home" : event.event_type === "arrived_home" ? "event-arrived_home" : ""}`}>{event.event_type === "left_home" ? <LogOut size={17} /> : event.event_type === "arrived_home" ? <House size={17} /> : <MessageCircle size={17} />}</div><div className="activity-main"><strong>{eventLabel(event)}</strong><small>{formatDateTime(event.occurred_at)}</small></div></div>) : <div className="empty-state"><div className="empty-icon">⌁</div><h2>{loading ? "履歴を読み込んでいます" : "履歴はまだありません"}</h2><p>外出や帰宅、メッセージ再生の記録がここに表示されます。</p></div>}</section>
    <div className="privacy-note"><MessageCircle size={16} /><span>ホーム画面では音声を再生できません。メッセージはデバイスから家主に届けられます。</span></div>
  </div>;
}
