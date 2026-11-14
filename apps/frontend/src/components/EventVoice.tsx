import { useEffect, useRef, useState } from "react";
import { Play } from "lucide-react";
import { fetchEventVoice } from "../api";

export default function EventVoice({ deviceId, eventId }: { deviceId: string; eventId: string }) {
  const [url, setUrl] = useState("");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const objectUrl = useRef("");
  const audio = useRef<HTMLAudioElement | null>(null);
  const request = useRef<AbortController | null>(null);
  useEffect(() => {
    const player = audio.current;
    return () => { player?.pause(); };
  }, [url]);
  useEffect(() => () => {
    request.current?.abort();
    if (objectUrl.current) URL.revokeObjectURL(objectUrl.current);
  }, []);
  const load = async () => {
    setLoading(true); setError("");
    const controller = new AbortController(); request.current = controller;
    try {
      const voice = await fetchEventVoice(deviceId, eventId, controller.signal);
      if (controller.signal.aborted) return;
      const next = URL.createObjectURL(voice);
      if (objectUrl.current) URL.revokeObjectURL(objectUrl.current);
      objectUrl.current = next; setUrl(next);
    } catch (error) {
      if (!controller.signal.aborted) setError(error instanceof Error ? error.message : "録音を読み込めませんでした。");
    } finally { if (!controller.signal.aborted) setLoading(false); }
  };
  return <div className="event-voice">
    {url ? <audio ref={audio} controls preload="metadata" src={url} aria-label="玄関で録音した返信" /> :
      <button className="button button-outline" onClick={() => void load()} disabled={loading}><Play size={14} />{loading ? "録音を読み込み中…" : "録音を開く"}</button>}
    {error && <small className="inline-alert error" role="alert">{error}</small>}
  </div>;
}
