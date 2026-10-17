import { useState } from "react";
import type { FormEvent } from "react";
import { CheckCircle2, KeyRound, LockKeyhole, LogIn, Sparkles, UserPlus } from "lucide-react";
import Robot from "../components/Robot";
import { login, register } from "../api";

type AuthMode = "login" | "register";

const EMAIL_PATTERN = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

export default function LoginPage({ onLogin }: { onLogin: () => void }) {
  const [mode, setMode] = useState<AuthMode>("login");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [passwordConfirmation, setPasswordConfirmation] = useState("");
  const [touched, setTouched] = useState({ email: false, password: false, confirmation: false });
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const [notice, setNotice] = useState("");

  const changeMode = (next: AuthMode) => {
    setMode(next); setError(""); setNotice("");
    setPassword(""); setPasswordConfirmation("");
    setTouched({ email: false, password: false, confirmation: false });
  };

  const isRegister = mode === "register";
  const normalizedEmail = email.trim();
  const emailValid = EMAIL_PATTERN.test(normalizedEmail);
  const passwordValid = password.length >= 8;
  const confirmationValid = passwordConfirmation === password && passwordConfirmation.length > 0;
  const emailHint = !touched.email
    ? "ログインに使うメールアドレスを入力してください。"
    : !normalizedEmail
      ? "メールアドレスを入力してください。"
      : !emailValid
        ? "メールアドレスの形を確認してください（例：you@example.com）。"
        : "メールアドレスの形はばっちりです。";
  const passwordHint = touched.password
    ? passwordValid ? "いい感じのパスワードです。" : `あと${Math.max(8 - password.length, 0)}文字以上入力してください。`
    : isRegister ? "8文字以上で設定してください。" : "パスワードを入力してください。";
  const confirmationHint = !touched.confirmation
    ? "確認のため、もう一度入力してください。"
    : confirmationValid
      ? "パスワードが一致しました。"
      : "パスワードが一致していません。";
  const canSubmit = emailValid && passwordValid && (!isRegister || confirmationValid);

  const submit = async (event: FormEvent) => {
    event.preventDefault(); setError(""); setNotice("");
    setTouched({ email: true, password: true, confirmation: true });
    if (!canSubmit) return;
    setBusy(true);
    try {
      if (mode === "register") {
        await register(normalizedEmail, password, passwordConfirmation);
        onLogin();
      } else {
        await login(normalizedEmail, password);
        onLogin();
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : mode === "register" ? "新規登録できませんでした。" : "ログインできませんでした。");
    } finally { setBusy(false); }
  };

  return <div className="login-screen">
    <section className="login-art">
      <div className="login-art-copy">
        <span className="eyebrow">A LITTLE VOICE FROM HOME</span>
        <h1>大切な人の声を、<br />いつもの場所へ。</h1>
        <p>あなたのメッセージが、帰宅したときの<br />小さな「おかえり」になります。</p>
      </div>
      <div className="login-speech"><span>おかえり、待ってたよ</span><i>♡</i></div>
      <Robot mode="idle" />
      <small className="login-note">A warm welcome, made by someone you love.</small>
    </section>
    <section className="login-panel">
      <div className="login-form-wrap">
        <div className="login-logo"><span className="logo-spark">⌁</span>ただいま。</div>
        <div className="login-welcome"><Sparkles size={15} /><span>あなたのおうちにつながろう</span></div>
        <p className="eyebrow">{isRegister ? "A NEW HELLO" : "WELCOME HOME"}</p>
        <h2>{isRegister ? "アカウントを作成" : "おかえりなさい"}</h2>
        <p className="muted">{isRegister ? "メールアドレスとパスワードを登録してください。" : "ログインして、大切な人の声を受け取りましょう。"}</p>
        <form className="login-form" onSubmit={submit} noValidate>
          <label htmlFor="login-email">メールアドレス
            <div className={`input-wrap ${touched.email && !emailValid ? "has-error" : touched.email && emailValid ? "is-valid" : ""}`}>
              <KeyRound size={17} aria-hidden="true" />
              <input
                id="login-email"
                type="email"
                value={email}
                onChange={e => { setEmail(e.target.value); setError(""); }}
                onBlur={() => setTouched(s => ({ ...s, email: true }))}
                autoComplete="email"
                autoCapitalize="none"
                autoCorrect="off"
                inputMode="email"
                placeholder="you@example.com"
                aria-invalid={touched.email && !emailValid}
                aria-describedby="email-hint"
                required
              />
            </div>
            <small id="email-hint" className={`field-hint ${touched.email && !emailValid ? "is-error" : touched.email && emailValid ? "is-valid" : ""}`}>
              {touched.email && emailValid && <CheckCircle2 size={13} aria-hidden="true" />}{emailHint}
            </small>
          </label>
          <label htmlFor="login-password">パスワード
            <div className={`input-wrap ${touched.password && !passwordValid ? "has-error" : touched.password && passwordValid ? "is-valid" : ""}`}>
              <LockKeyhole size={17} aria-hidden="true" />
              <input
                id="login-password"
                type="password"
                value={password}
                onChange={e => { setPassword(e.target.value); setError(""); }}
                onBlur={() => setTouched(s => ({ ...s, password: true }))}
                autoComplete={isRegister ? "new-password" : "current-password"}
                placeholder={isRegister ? "8文字以上" : "パスワードを入力"}
                minLength={8}
                aria-invalid={touched.password && !passwordValid}
                aria-describedby="password-hint"
                required
              />
            </div>
            <small id="password-hint" className={`field-hint ${touched.password && !passwordValid ? "is-error" : touched.password && passwordValid ? "is-valid" : ""}`}>
              {touched.password && passwordValid && <CheckCircle2 size={13} aria-hidden="true" />}{passwordHint}
            </small>
          </label>
          {isRegister && <label htmlFor="login-password-confirmation">パスワード（確認）
            <div className={`input-wrap ${touched.confirmation && !confirmationValid ? "has-error" : confirmationValid ? "is-valid" : ""}`}>
              <LockKeyhole size={17} aria-hidden="true" />
              <input
                id="login-password-confirmation"
                type="password"
                value={passwordConfirmation}
                onChange={e => { setPasswordConfirmation(e.target.value); setError(""); }}
                onBlur={() => setTouched(s => ({ ...s, confirmation: true }))}
                autoComplete="new-password"
                placeholder="もう一度入力"
                minLength={8}
                aria-invalid={touched.confirmation && !confirmationValid}
                aria-describedby="confirmation-hint"
                required
              />
            </div>
            <small id="confirmation-hint" className={`field-hint ${touched.confirmation && !confirmationValid ? "is-error" : confirmationValid ? "is-valid" : ""}`}>
              {confirmationValid && <CheckCircle2 size={13} aria-hidden="true" />}{confirmationHint}
            </small>
          </label>}
          {error && <p className="form-error" role="alert">{error}</p>}
          {notice && <p className="form-notice" role="status">{notice}</p>}
          <button className="button button-primary login-submit" type="submit" disabled={busy || !canSubmit}>
            {isRegister ? <UserPlus size={16} /> : <LogIn size={16} />}{busy ? "処理中…" : isRegister ? "アカウントを作成" : "ログインしてつながる"}
          </button>
        </form>
        <div className="auth-switch">{isRegister ? "すでにアカウントをお持ちですか？" : "はじめての方はこちら"} <button type="button" className="text-button" onClick={() => changeMode(isRegister ? "login" : "register")}>{isRegister ? "ログイン" : "新規登録"}</button></div>
        <div className="demo-hint">メールアドレスはアカウントへのログインに使います。パスワードは安全に保管されます。</div>
      </div>
    </section>
  </div>;
}
