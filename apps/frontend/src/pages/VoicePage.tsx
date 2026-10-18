import { useEffect, useRef, useState } from "react";
import { Check, CircleStop, Download, House, LogOut, Mic, Music2, Square, Trash2, Volume2 } from "lucide-react";
import { blobToWav16kMono, formatBytes } from "../audio";
import { apiRequest, devicePath } from "../api";
import { useDevices } from "../devices";

const MAX_SECONDS = 5;
const MAX_BYTES = 200 * 1024;
const MAX_WAV_BYTES = 44 + 16000 * 2 * MAX_SECONDS;
type AudioSlot = "leave" | "arrive";
const AUDIO_SLOTS: { id: AudioSlot; label: string; phrase: string }[] = [
  { id: "leave", label: "外出", phrase: "いってらっしゃい" },
  { id: "arrive", label: "帰宅", phrase: "おかえり" },
];

export default function VoicePage() {
  const { active } = useDevices();
  const [recording, setRecording] = useState(false);
  const [seconds, setSeconds] = useState(0);
  const [audio, setAudio] = useState<Blob | null>(null);
  const [audioUrl, setAudioUrl] = useState("");
  const [sending, setSending] = useState(false);
  const [message, setMessage] = useState("");
  const [senderName, setSenderName] = useState("");
  const [audioSlot, setAudioSlot] = useState<AudioSlot>("arrive");
  const [isPrepared, setIsPrepared] = useState(false);
  const [wavBlob, setWavBlob] = useState<Blob | null>(null);
  const [micError, setMicError] = useState("");
  const mediaRecorder = useRef<MediaRecorder | null>(null);
  const streamRef = useRef<MediaStream | null>(null);
  const chunks = useRef<BlobPart[]>([]);
  const timer = useRef<number | undefined>(undefined);
  const secondsRef = useRef(0);

  useEffect(() => () => {
    if (timer.current) window.clearInterval(timer.current);
    streamRef.current?.getTracks().forEach(track => track.stop());
    if (audioUrl) URL.revokeObjectURL(audioUrl);
  }, [audioUrl]);

  const stopRecording = () => {
    if (mediaRecorder.current?.state === "recording") mediaRecorder.current.stop();
    if (timer.current) window.clearInterval(timer.current);
    setRecording(false);
    streamRef.current?.getTracks().forEach(track => track.stop());
    streamRef.current = null;
  };

  const startRecording = async () => {
    setMicError(""); setMessage(""); setIsPrepared(false); setWavBlob(null); setSeconds(0); secondsRef.current = 0;
    if (!navigator.mediaDevices?.getUserMedia || !window.MediaRecorder) {
      setMicError("このブラウザは録音に対応していません。最新版のChromeなどでお試しください。");
      return;
    }
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: { channelCount: 1, echoCancellation: true, noiseSuppression: true } });
      streamRef.current = stream;
      const recorder = new MediaRecorder(stream);
      mediaRecorder.current = recorder;
      chunks.current = [];
      recorder.ondataavailable = event => { if (event.data.size > 0) chunks.current.push(event.data); };
      recorder.onstop = () => {
        const blob = new Blob(chunks.current, { type: recorder.mimeType || "audio/webm" });
        if (audioUrl) URL.revokeObjectURL(audioUrl);
        setAudio(blob);
        setAudioUrl(URL.createObjectURL(blob));
        setIsPrepared(false);
      };
      recorder.start();
      setRecording(true);
      timer.current = window.setInterval(() => {
        secondsRef.current += 1;
        setSeconds(secondsRef.current);
        if (secondsRef.current >= MAX_SECONDS) stopRecording();
      }, 1000);
    } catch {
      setMicError("マイクを利用できません。ブラウザのマイク許可とHTTPS接続を確認してください。");
    }
  };

  const clearAudio = () => {
    if (audioUrl) URL.revokeObjectURL(audioUrl);
    setAudio(null); setAudioUrl(""); setSeconds(0); setMessage(""); setIsPrepared(false); setWavBlob(null);
  };

  const prepareWav = async () => {
    if (!audio) return;
    setSending(true); setMessage(""); setIsPrepared(false);
    try {
      if (audio.size === 0) throw new Error("録音データが空です。もう一度録音してください。");
      if (seconds > MAX_SECONDS) throw new Error("録音は5秒以内にしてください。");
      const wav = await blobToWav16kMono(audio);
      if (wav.size > MAX_BYTES) throw new Error("WAVファイルは200KB以下にしてください。");
      if (wav.size > MAX_WAV_BYTES) throw new Error("変換後の音声が5秒を超えています。5秒以内で録音してください。");
      setWavBlob(wav); setIsPrepared(true);
      setMessage(`WAV形式に変換しました（${formatBytes(wav.size)}）。内容を確認してメッセージとして登録できます。`);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "音声の変換に失敗しました。");
    } finally { setSending(false); }
  };

  const downloadWav = () => {
    if (!wavBlob) return;
    const url = URL.createObjectURL(wavBlob);
    const link = document.createElement("a"); link.href = url; link.download = "tadaima-message.wav";
    document.body.appendChild(link); link.click(); link.remove(); window.setTimeout(() => URL.revokeObjectURL(url), 1000);
  };

  const selectedSlot = AUDIO_SLOTS.find(slot => slot.id === audioSlot) ?? AUDIO_SLOTS[1];
  const playback = audioSlot === "leave" ? "外出するときに「いってらっしゃい」として再生されます。" : "帰宅したときに「おかえり」として再生されます。";

  const selectSlot = (slot: AudioSlot) => {
    setAudioSlot(slot);
    if (isPrepared) { setMessage(""); setIsPrepared(false); }
  };

  const uploadVoice = async () => {
    if (!audio) return;
    setSending(true); setMessage(""); setIsPrepared(false);
    try {
      const wav = wavBlob || await blobToWav16kMono(audio);
      if (wav.size > MAX_BYTES) throw new Error("WAVファイルは200KB以下にしてください。");
      if (wav.size > MAX_WAV_BYTES) throw new Error("変換後の音声が5秒を超えています。5秒以内で録音してください。");
      setWavBlob(wav);
      const form = new FormData();
      if (!senderName.trim()) throw new Error("登録者名（ニックネーム）を入力してください。");
      form.append("file", wav, `${audioSlot}-message.wav`);
      form.append("sender_name", senderName.trim());
      form.append("slot", audioSlot);
      const result = await apiRequest<{ file_name?: string; version?: number; byte_size?: number }>(devicePath("/message"), { method: "POST", body: form });
      setIsPrepared(true);
      setMessage(`「${active?.name ?? ""}」に${selectedSlot.label}用の音声を登録しました${result?.version ? `（バージョン ${result.version}）` : ""}。${playback}`);
    } catch (error) {
      setMessage(error instanceof Error && error.message !== "Failed to fetch" ? error.message : "音声を登録できませんでした。接続先と入力内容を確認してください。");
    } finally { setSending(false); }
  };

  return <div className="page-content voice-page">
    <div className="page-heading"><div><p className="eyebrow">A MESSAGE FROM SOMEONE YOU LOVE</p><h1>声を届ける</h1><p className="muted">大切な人の声を、いつもの場所に。</p></div><div className="heading-icon voice-heading-icon"><Mic size={21} /></div></div>
    <section className="voice-intro"><div className="voice-intro-icon"><Music2 size={22} /></div><div><strong>大切な人に届けるメッセージを録音</strong><p>家族や友達からのメッセージを登録すると、{playback}</p></div></section>

    <div className="voice-layout">
      <section className="voice-card">
        <div className="voice-card-top"><span className="eyebrow">VOICE RECORDER</span><span className={`record-status ${recording ? "is-recording" : ""}`}><i />{recording ? "録音中" : audio ? "録音済み" : "待機中"}</span></div>
        <div className={`record-orb ${recording ? "is-recording" : ""}`}><div className="orb-ring orb-ring-1" /><div className="orb-ring orb-ring-2" /><div className="orb-core"><Mic size={31} /></div></div>
        <div className="record-time">{`00:${String(seconds).padStart(2, "0")}`}<small> / 00:05</small></div>
        <div className="waveform" aria-label="音声波形の装飾">{Array.from({ length: 39 }, (_, i) => <i key={i} style={{ height: `${10 + ((i * 17 + 13) % 27)}px`, animationDelay: `${i * 0.025}s` }} />)}</div>
        <p className="record-hint">{recording ? "話し終わったら停止ボタンを押してください。" : audio ? "録音を確認して、メッセージとして登録できます。" : "録音ボタンを押して、メッセージを話してください。"}</p>
        {micError && <div className="inline-alert error">{micError}</div>}
        <div className="record-actions">{!recording ? <button className="button button-primary record-button" onClick={startRecording} disabled={sending}><Mic size={18} />{audio ? "録り直す" : "録音を開始"}</button> : <button className="button button-danger record-button" onClick={stopRecording}><Square size={16} fill="currentColor" />録音を停止</button>}</div>
        {recording && <p className="recording-warning"><span />最大5秒で自動停止します</p>}
      </section>
      <section className="preview-card">
        <div className="section-heading"><div><h2>メッセージの確認</h2><p className="muted">保存前に再生して確認できます。</p></div><div className="preview-icon"><Volume2 size={18} /></div></div>
        {audio && audioUrl ? <div className="audio-preview"><div className="audio-file-icon"><Music2 size={20} /></div><div className="audio-file-info"><strong>録音したメッセージ</strong><small>{formatBytes(audio.size)} ・ 元の録音データ</small></div><audio controls src={audioUrl} /></div> : <div className="preview-empty"><div className="preview-empty-icon"><Mic size={23} /></div><strong>まだ録音されていません</strong><p>録音すると、ここで試聴できます。</p></div>}
        <div className="voice-target">登録先：<strong>{active?.name}</strong><small>{active?.id}</small></div>
        <div className="slot-choice" role="radiogroup" aria-label="再生するタイミング">{AUDIO_SLOTS.map(slot => <button type="button" key={slot.id} role="radio" aria-checked={audioSlot === slot.id} className={audioSlot === slot.id ? "is-selected" : ""} onClick={() => selectSlot(slot.id)} disabled={sending || recording}>{slot.id === "leave" ? <LogOut size={16} /> : <House size={16} />}<span><strong>{slot.label}</strong><small>{slot.phrase}</small></span></button>)}</div>
        <label className="sender-name-field">登録者名（ニックネーム）<input value={senderName} onChange={e => setSenderName(e.target.value)} maxLength={40} placeholder="例：お母さん、ゆうた" required /></label>
        <div className="format-info"><span>変換後の形式</span><strong>WAV · PCM 16bit · 16 kHz · モノラル</strong><small>最大5秒 / 200 KB以下</small></div>
        <div className="preview-actions">
          <button className="button button-outline" onClick={clearAudio} disabled={!audio || recording || sending}><Trash2 size={16} />クリア</button>
          <button className="button button-outline" onClick={prepareWav} disabled={!audio || recording || sending}>{sending ? <><span className="button-spinner" />処理中…</> : <><Check size={17} />WAV変換</>}</button>
          <button className="button button-primary" onClick={() => void uploadVoice()} disabled={!audio || !senderName.trim() || recording || sending}>{sending ? <><span className="button-spinner" />登録中…</> : <><Volume2 size={17} />メッセージを登録</>}</button>
        </div>
        {message && <div className={`inline-alert ${isPrepared ? "success" : "error"}`}>{message}</div>}
        {wavBlob && <button className="text-button" onClick={downloadWav}><Download size={14} /> WAVファイルをダウンロード</button>}
      </section>
    </div>
    <div className="voice-safety"><CircleStop size={17} /><span>登録したメッセージはサーバーに保存され、{playback}Home画面では再生できません。</span></div>
  </div>;
}
