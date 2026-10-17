import { useState } from "react";
import type { FormEvent } from "react";
import { KeyRound, LockKeyhole, LogIn } from "lucide-react";
import Robot from "../components/Robot";
import { login } from "../api";
import { getActiveDeviceId } from "../devices";

export default function LoginPage({ onLogin }: { onLogin: () => void }) {
  const [deviceId, setDeviceId] = useState(getActiveDeviceId());
  const [password, setPassword] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const showDemoCredentials = ["localhost", "127.0.0.1"].includes(window.location.hostname);
  const submit = async (event: FormEvent) => {
    event.preventDefault(); setBusy(true); setError("");
    try { await login(deviceId, password); onLogin(); }
    catch (e) { setError(e instanceof Error ? e.message : "ログインできませんでした。"); }
    finally { setBusy(false); }
  };
  return <div className="login-screen">
    <section className="login-art"><div className="login-art-copy"><span className="eyebrow">A LITTLE VOICE FROM HOME</span><h1>大切な人の声を、<br />いつもの場所へ。</h1><p>あなたのメッセージが、帰宅したときの<br />小さな「おかえり」になります。</p></div><Robot mode="idle" /><small className="login-note">A warm welcome, made by someone you love.</small></section>
    <section className="login-panel"><div className="login-form-wrap"><div className="login-logo"><span className="logo-spark">⌁</span>ただいま。</div><p className="eyebrow">DEVICE ACCESS</p><h2>メッセージを届ける</h2><p className="muted">デバイスIDとパスワードを入力してください。</p>
      <form className="login-form" onSubmit={submit}>
        <label>デバイスID<div className="input-wrap"><KeyRound size={17} /><input value={deviceId} onChange={e => setDeviceId(e.target.value)} autoComplete="username" required /></div></label>
        <label>パスワード<div className="input-wrap"><LockKeyhole size={17} /><input type="password" value={password} onChange={e => setPassword(e.target.value)} autoComplete="current-password" placeholder="パスワードを入力" required /></div></label>
        {error && <p className="form-error" role="alert">{error}</p>}
        <button className="button button-primary" type="submit" disabled={busy || !deviceId.trim() || !password}><LogIn size={16} />{busy ? "確認中…" : "ログイン"}</button>
      </form><div className="demo-hint">{showDemoCredentials ? <>ローカル確認用: デバイスID <code>entrance-1</code> / パスワード <code>tadaima-demo</code></> : "家主から共有されたデバイスIDとパスワードを使用してください。"}</div>
    </div></section>
  </div>;
}
