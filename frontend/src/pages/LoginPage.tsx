import { useState } from "react";
import type { FormEvent } from "react";
import { KeyRound, LockKeyhole, LogIn, UserPlus } from "lucide-react";
import Robot from "../components/Robot";
import { login, register } from "../api";

type AuthMode = "login" | "register";

export default function LoginPage({ onLogin }: { onLogin: () => void }) {
  const [mode, setMode] = useState<AuthMode>("login");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [passwordConfirmation, setPasswordConfirmation] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const [notice, setNotice] = useState("");

  const changeMode = (next: AuthMode) => {
    setMode(next); setError(""); setNotice("");
    setPassword(""); setPasswordConfirmation("");
  };

  const submit = async (event: FormEvent) => {
    event.preventDefault(); setBusy(true); setError(""); setNotice("");
    if (mode === "register" && password !== passwordConfirmation) {
      setError("確認用パスワードが一致しません。"); setBusy(false); return;
    }
    try {
      if (mode === "register") {
        await register(email, password, passwordConfirmation);
        onLogin();
      } else {
        await login(email, password);
        onLogin();
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : mode === "register" ? "新規登録できませんでした。" : "ログインできませんでした。");
    } finally { setBusy(false); }
  };

  const isRegister = mode === "register";
  return <div className="login-screen">
    <section className="login-art"><div className="login-art-copy"><span className="eyebrow">A LITTLE VOICE FROM HOME</span><h1>大切な人の声を、<br />いつもの場所へ。</h1><p>あなたのメッセージが、帰宅したときの<br />小さな「おかえり」になります。</p></div><Robot mode="idle" /><small className="login-note">A warm welcome, made by someone you love.</small></section>
    <section className="login-panel"><div className="login-form-wrap"><div className="login-logo"><span className="logo-spark">⌁</span>ただいま。</div><p className="eyebrow">DEVICE ACCESS</p><h2>{isRegister ? "アカウントを作成" : "メッセージを届ける"}</h2><p className="muted">{isRegister ? "メールアドレスとパスワードを登録してください。" : "登録したメールアドレスとパスワードでログインしてください。"}</p>
      <form className="login-form" onSubmit={submit}>
        <label>メールアドレス<div className="input-wrap"><KeyRound size={17} /><input type="email" value={email} onChange={e => setEmail(e.target.value)} autoComplete="username" placeholder="you@example.com" required /></div></label>
        <label>パスワード<div className="input-wrap"><LockKeyhole size={17} /><input type="password" value={password} onChange={e => setPassword(e.target.value)} autoComplete={isRegister ? "new-password" : "current-password"} placeholder={isRegister ? "8文字以上" : "パスワードを入力"} minLength={8} required /></div></label>
        {isRegister && <label>パスワード（確認）<div className="input-wrap"><LockKeyhole size={17} /><input type="password" value={passwordConfirmation} onChange={e => setPasswordConfirmation(e.target.value)} autoComplete="new-password" placeholder="もう一度入力" minLength={8} required /></div></label>}
        {error && <p className="form-error" role="alert">{error}</p>}
        {notice && <p className="form-notice" role="status">{notice}</p>}
        <button className="button button-primary" type="submit" disabled={busy || !email.trim() || !password || (isRegister && !passwordConfirmation)}>{isRegister ? <UserPlus size={16} /> : <LogIn size={16} />}{busy ? "処理中…" : isRegister ? "新規登録" : "ログイン"}</button>
      </form>
      <div className="auth-switch">{isRegister ? "すでにアカウントをお持ちですか？" : "アカウントをお持ちでない方"} <button type="button" className="text-button" onClick={() => changeMode(isRegister ? "login" : "register")}>{isRegister ? "ログイン" : "新規登録"}</button></div>
      <div className="demo-hint">登録したアカウントはメールアドレスで管理されます。パスワードは安全にハッシュ化してサーバー側で保存してください。</div>
    </div></section>
  </div>;
}
