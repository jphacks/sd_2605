import { useState } from "react";
import type { FormEvent } from "react";
import { Link } from "react-router-dom";
import { KeyRound, Mic, Send } from "lucide-react";
import { acceptDeviceInvitation, ApiError } from "../api";
import { getDeviceName } from "../devices";

/** 差し出し手向け：渡された招待コードを入力して、その家に声を届けられるようにする */
export default function ReceivePage() {
  const [code, setCode] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const [joined, setJoined] = useState("");

  const submit = async (event: FormEvent) => {
    event.preventDefault(); setError(""); setJoined(""); setBusy(true);
    try {
      const { device_id } = await acceptDeviceInvitation(code);
      setCode(""); setJoined(device_id);
    } catch (e) {
      setError(e instanceof ApiError && e.code === "already_resident" ? "この家の居住者は、差し出し手にはなれません。"
        : e instanceof ApiError && e.code === "invalid_invitation" ? "この招待コードは使えません。期限切れ・使用済み・入力ミスがないか確認してください。"
          : e instanceof Error ? e.message : "招待を受け取れませんでした。");
    } finally { setBusy(false); }
  };

  return <div className="page-content">
    <div className="page-heading"><div><p className="eyebrow">JOIN A HOME</p><h1>招待を受け取る</h1><p className="muted">家の人から受け取った招待コードを入力すると、その家に声を届けられます。</p></div><div className="heading-icon"><Send size={21} /></div></div>
    {error && <div className="inline-alert error" role="alert">{error}</div>}
    {joined && <div className="inline-alert success" role="status">招待を受け取りました。「{getDeviceName(joined)}」を操作中にしました。<Link className="text-button" to="/voice"><Mic size={14} />メッセージを登録する</Link></div>}
    <form className="settings-card device-add-form invitation-accept-form" onSubmit={e => void submit(e)}>
      <label className="sender-name-field" htmlFor="invitation-code">招待コード
        <input id="invitation-code" value={code} onChange={e => setCode(e.target.value.toUpperCase().replace(/\s/g, ""))} placeholder="8文字のコード" autoComplete="off" autoCapitalize="characters" spellCheck={false} minLength={8} maxLength={8} pattern="[A-HJ-NP-Z2-9]{8}" aria-describedby="invitation-code-hint" required />
        <small id="invitation-code-hint" className="device-field-help">英大文字と数字の8文字です（O・I・0・1は使われません）。</small>
      </label>
      <button className="button button-primary device-submit" type="submit" disabled={busy || code.trim().length !== 8}><KeyRound size={16} />{busy ? "確認しています…" : "招待を受け取る"}</button>
    </form>
    <div className="privacy-note"><KeyRound size={16} /><span>コードは24時間のあいだ、1回だけ使えます。受け取ると、この家に音声メッセージを登録できるようになります。</span></div>
  </div>;
}
