import { useState } from "react";
import { Check, Clock3, Copy, KeyRound, ShieldCheck, UserPlus } from "lucide-react";
import { ApiError, createDeviceInvitation, DeviceInvitation, formatDateTime } from "../api";
import { useDevices } from "../devices";

/** 居住者向け：招待コードを発行して、家族・友人（差し出し手）に直接渡す */
export default function InvitePage() {
  const { active } = useDevices();
  const [invitation, setInvitation] = useState<DeviceInvitation | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const [copied, setCopied] = useState(false);

  const issue = async () => {
    setBusy(true); setError(""); setCopied(false);
    try { setInvitation(await createDeviceInvitation(active?.id)); }
    catch (e) {
      setInvitation(null);
      setError(e instanceof ApiError && e.code === "resident_only" ? "招待コードを発行できるのは、この家の居住者だけです。" : e instanceof Error ? e.message : "招待コードを発行できませんでした。");
    } finally { setBusy(false); }
  };
  const copy = async () => {
    if (!invitation) return;
    try { await navigator.clipboard.writeText(invitation.code); setCopied(true); window.setTimeout(() => setCopied(false), 2000); }
    catch { setError("コピーできませんでした。コードを選択してコピーしてください。"); }
  };

  return <div className="page-content">
    <div className="page-heading"><div><p className="eyebrow">INVITE SOMEONE</p><h1>家族・友人を招待</h1><p className="muted">{active?.name} に声を届けてくれる人へ、招待コードを渡します。</p></div><div className="heading-icon"><UserPlus size={21} /></div></div>
    {error && <div className="inline-alert error" role="alert">{error}</div>}
    <section className="settings-card invite-card">
      <div className="settings-card-heading"><div className="settings-icon"><KeyRound size={19} /></div><div><h2>招待コード</h2><p>コードを受け取った人は、この家に音声メッセージを登録できます。</p></div></div>
      {invitation
        ? <div className="invite-code-box" role="status">
          <span>{active?.name} への招待コード</span>
          <strong aria-label={`招待コード ${invitation.code.split("").join(" ")}`}>{invitation.code}</strong>
          <small><Clock3 size={12} /> 有効期限：{formatDateTime(invitation.expires_at)}（1回だけ使えます）</small>
          <button type="button" className="button button-outline" onClick={() => void copy()}>{copied ? <><Check size={15} />コピーしました</> : <><Copy size={15} />コードをコピー</>}</button>
        </div>
        : <div className="invite-empty"><p>まだコードを発行していません。</p></div>}
      <button type="button" className="button button-primary device-submit" onClick={() => void issue()} disabled={busy || !active}><KeyRound size={16} />{busy ? "発行しています…" : invitation ? "新しいコードを発行する" : "招待コードを発行する"}</button>
    </section>
    <section className="settings-card invite-notes">
      <h2>渡し方と注意</h2>
      <ul>
        <li>コードはメールでは送られません。メッセージアプリや口頭などで、直接渡してください。</li>
        <li>有効期限は発行から24時間で、使えるのは1回だけです。別の人には、その都度新しく発行してください。</li>
        <li>受け取った人は「差し出し手」になり、音声メッセージの登録ができます。デバイスの設定は変更できません。</li>
      </ul>
    </section>
    <div className="privacy-note"><ShieldCheck size={16} /><span>差し出し手には、外出・帰宅のお知らせが登録メールアドレスへ届きます。</span></div>
  </div>;
}
