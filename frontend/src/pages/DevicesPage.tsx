import { useCallback, useEffect, useState } from "react";
import type { FormEvent, KeyboardEvent } from "react";
import { Cpu, KeyRound, LockKeyhole, Pencil, Plus, RefreshCw, Trash2, Check, X } from "lucide-react";
import { apiRequest, devicePath, loginDevice, registerDevice, DeviceStatus, formatDateTime, ApiError } from "../api";
import { DEVICE_ID_PATTERN, removeDevice, renameDevice, setActiveDevice, useDevices } from "../devices";

type Info = { state: "loading" } | { state: "ok"; status: DeviceStatus } | { state: "error"; httpStatus?: number; message: string };
type DeviceTab = "login" | "register";

export default function DevicesPage() {
  const { devices, activeId, hasSession } = useDevices();
  const [tab, setTab] = useState<DeviceTab>("login");
  const [info, setInfo] = useState<Record<string, Info>>({});
  const [error, setError] = useState("");
  const [message, setMessage] = useState("");
  const [busy, setBusy] = useState(false);
  const [existingId, setExistingId] = useState("");
  const [existingPassword, setExistingPassword] = useState("");
  const [newId, setNewId] = useState("");
  const [newName, setNewName] = useState("");
  const [newPassword, setNewPassword] = useState("");
  const [newPasswordConfirmation, setNewPasswordConfirmation] = useState("");
  const [editing, setEditing] = useState<string | null>(null);
  const [draft, setDraft] = useState("");
  const [reauth, setReauth] = useState<string | null>(null);
  const [reauthPassword, setReauthPassword] = useState("");

  // ログイン済みデバイスの状態をまとめて取得（1台の失敗が他の台に影響しない）
  const loggedIn = devices.filter(d => hasSession(d.id)).map(d => d.id);
  const loggedInKey = loggedIn.join(",");
  const load = useCallback(async () => {
    const ids = loggedInKey ? loggedInKey.split(",") : [];
    setInfo(Object.fromEntries(ids.map(id => [id, { state: "loading" } as Info])));
    await Promise.all(ids.map(async id => {
      try { const status = await apiRequest<DeviceStatus>(devicePath("/status", id)); setInfo(prev => ({ ...prev, [id]: { state: "ok", status } })); }
      catch (e) {
        setInfo(prev => ({ ...prev, [id]: {
          state: "error", httpStatus: e instanceof ApiError ? e.status : undefined,
          message: e instanceof Error ? e.message : "通信に失敗しました。接続を確認して更新してください。",
        } }));
      }
    }));
  }, [loggedInKey]);
  useEffect(() => { void load(); }, [load]);

  const addDeviceSubmit = async (event: FormEvent) => {
    event.preventDefault(); setError(""); setMessage("");
    const id = newId.trim();
    if (!DEVICE_ID_PATTERN.test(id)) { setError("デバイスIDは英数字・ハイフン・アンダースコアで入力してください。"); return; }
    if (newPassword.length < 8) { setError("パスワードは8文字以上にしてください。"); return; }
    if (newPasswordConfirmation !== newPassword) { setError("確認用パスワードが一致しません。"); return; }
    setBusy(true);
    try {
      await registerDevice(id, newPassword);
      if (newName.trim()) renameDevice(id, newName);
      setMessage(`「${newName.trim() || id}」を登録し、このデバイスを操作中にしました。`);
      setNewId(""); setNewName(""); setNewPassword(""); setNewPasswordConfirmation("");
    } catch (e) { setError(e instanceof Error ? e.message : "デバイスを登録できませんでした。"); }
    finally { setBusy(false); }
  };

  const addExistingDeviceSubmit = async (event: FormEvent) => {
    event.preventDefault(); setError(""); setMessage("");
    const id = existingId.trim();
    if (!DEVICE_ID_PATTERN.test(id)) { setError("デバイスIDは英数字・ハイフン・アンダースコアで入力してください。"); return; }
    if (!existingPassword) { setError("登録済みデバイスのパスワードを入力してください。"); return; }
    setBusy(true);
    try {
      await loginDevice(id, existingPassword);
      setMessage(`登録済みデバイス「${id}」を追加し、操作中にしました。`);
      setExistingId(""); setExistingPassword("");
    } catch (e) { setError(e instanceof Error ? e.message : "登録済みデバイスを追加できませんでした。"); }
    finally { setBusy(false); }
  };

  const reauthSubmit = async (event: FormEvent, id: string) => {
    event.preventDefault(); setError(""); setMessage(""); setBusy(true);
    try { await loginDevice(id, reauthPassword); setReauth(null); setReauthPassword(""); }
    catch (e) { setError(e instanceof Error ? e.message : "ログインできませんでした。"); }
    finally { setBusy(false); }
  };

  const saveName = (id: string) => { renameDevice(id, draft); setEditing(null); };
  const remove = (id: string, name: string) => {
    if (window.confirm(`「${name}」を一覧から外し、ログアウトしますか？\nデバイス本体やサーバー上のデータは削除されません。`)) { removeDevice(id); setMessage(`「${name}」を一覧から外しました。`); }
  };
  const statusPill = (id: string) => {
    if (!hasSession(id)) return <span className="status-pill device-status-needs-login"><span className="status-dot is-offline" />ログインが必要</span>;
    const i = info[id];
    if (!i || i.state === "loading") return <span className="status-pill device-status-loading">状態を確認中</span>;
    if (i.state === "error") return <span className="status-pill device-status-error"><span className="status-dot is-offline" />状態を確認できません</span>;
    return <span className={`status-pill ${i.status.online ? "device-status-online" : "device-status-offline"}`}><span className={`status-dot ${i.status.online ? "" : "is-offline"}`} />{i.status.online ? "接続中" : "デバイス未接続"}</span>;
  };

  const selectTab = (nextTab: DeviceTab) => {
    setTab(nextTab);
    setError("");
    setMessage("");
    setReauth(null);
    setReauthPassword("");
  };
  const handleTabKeyDown = (event: KeyboardEvent<HTMLButtonElement>) => {
    if (!["ArrowLeft", "ArrowRight", "Home", "End"].includes(event.key)) return;
    event.preventDefault();
    const tabs = Array.from(event.currentTarget.parentElement?.querySelectorAll<HTMLButtonElement>('[role="tab"]') ?? []);
    const nextIndex = event.key === "Home" ? 0
      : event.key === "End" ? tabs.length - 1
        : (tabs.indexOf(event.currentTarget) + (event.key === "ArrowRight" ? 1 : -1) + tabs.length) % tabs.length;
    const nextTab = tabs[nextIndex];
    nextTab?.focus();
    if (nextTab?.id === "device-login-tab") selectTab("login");
    if (nextTab?.id === "device-register-tab") selectTab("register");
  };

  return <div className="page-content">
    <div className="page-heading"><div><p className="eyebrow">MY ROBOTS</p><h1>デバイス管理</h1><p className="muted">すでに登録済みのデバイスを追加するか、新しいデバイスを登録できます。</p></div></div>
    {error && <div className="inline-alert error" role="alert">{error}</div>}
    {message && <div className="inline-alert success" role="status">{message}</div>}

    <div className="device-tabs" role="tablist" aria-label="デバイスの追加方法">
      <button
        id="device-login-tab"
        className={`device-tab ${tab === "login" ? "is-active" : ""}`}
        type="button"
        role="tab"
        aria-selected={tab === "login"}
        aria-controls="device-login-panel"
        tabIndex={tab === "login" ? 0 : -1}
        onClick={() => selectTab("login")}
        onKeyDown={handleTabKeyDown}
      >
        <span className="device-tab-icon"><LockKeyhole size={17} /></span>
        <span className="device-tab-copy"><strong>登録済みのデバイス</strong><small>IDとパスワードを入力して追加</small></span>
      </button>
      <button
        id="device-register-tab"
        className={`device-tab ${tab === "register" ? "is-active" : ""}`}
        type="button"
        role="tab"
        aria-selected={tab === "register"}
        aria-controls="device-register-panel"
        tabIndex={tab === "register" ? 0 : -1}
        onClick={() => selectTab("register")}
        onKeyDown={handleTabKeyDown}
      >
        <span className="device-tab-icon"><Plus size={17} /></span>
        <span className="device-tab-copy"><strong>新しいデバイス</strong><small>IDとパスワードを新規登録</small></span>
      </button>
    </div>

    <section id="device-login-panel" className="device-tab-panel" role="tabpanel" aria-labelledby="device-login-tab" hidden={tab !== "login"}>
      <div className="device-panel-intro">
        <span className="device-panel-icon"><KeyRound size={19} /></span>
        <div><h2>登録済みデバイスを追加</h2><p>デバイス本体の設定時に登録したIDとパスワードを入力してください。認証後、このアカウントの一覧に追加され、利用できるようになります。</p></div>
      </div>
      <form className="settings-card device-add-form" onSubmit={e => void addExistingDeviceSubmit(e)}>
        <label className="sender-name-field" htmlFor="existing-device-id">デバイスID
          <input id="existing-device-id" value={existingId} onChange={e => setExistingId(e.target.value)} placeholder="例：entrance-1" autoComplete="off" autoCapitalize="none" autoCorrect="off" spellCheck={false} minLength={1} maxLength={64} pattern="[A-Za-z0-9_-]+" aria-describedby="existing-device-id-hint" required />
          <small id="existing-device-id-hint" className="device-field-help">英数字、ハイフン、アンダースコアで入力します。</small>
        </label>
        <label className="sender-name-field" htmlFor="existing-device-password">デバイスのパスワード
          <input id="existing-device-password" type="password" value={existingPassword} onChange={e => setExistingPassword(e.target.value)} placeholder="登録時に設定したパスワード" autoComplete="current-password" required />
        </label>
        <button className="button button-primary device-submit" type="submit" disabled={busy || !existingId.trim() || !existingPassword}><LockKeyhole size={16} />{busy ? "認証しています…" : "このデバイスを追加"}</button>
      </form>
      <section className="device-saved-section" aria-label="このアカウントのデバイス">
        <div className="section-heading device-panel-heading">
          <div><h2>このアカウントのデバイス</h2><p className="muted">このアカウントで追加したデバイスだけを表示します。別のブラウザでは、IDとパスワードでもう一度追加してください。</p></div>
          <button className="button button-outline" onClick={() => void load()}><RefreshCw size={15} />状態を更新</button>
        </div>
        {devices.length === 0
          ? <div className="activity-card device-empty-state"><Cpu size={24} /><p>このアカウントには、まだデバイスがありません。</p><span>上のフォームから登録済みデバイスを追加してください。</span></div>
          : <section className="activity-card device-list" aria-label="追加済みデバイス一覧">
            {devices.map(d => {
              const i = info[d.id]; const authed = hasSession(d.id); const isActive = d.id === activeId;
              return <div className={`activity-row device-row ${isActive ? "is-active" : ""}`} key={d.id}>
                <div className="event-symbol"><Cpu size={17} /></div>
                <div className="activity-main">
                  {editing === d.id
                    ? <div className="device-rename"><input autoFocus aria-label="表示名" value={draft} maxLength={30} onChange={e => setDraft(e.target.value)} onKeyDown={e => { if (e.key === "Enter") saveName(d.id); if (e.key === "Escape") setEditing(null); }} /><button className="text-button" onClick={() => saveName(d.id)} aria-label="名前を保存"><Check size={16} /></button><button className="text-button" onClick={() => setEditing(null)} aria-label="キャンセル"><X size={16} /></button></div>
                    : <strong>{d.name}{isActive && <span className="device-badge">操作中</span>}</strong>}
                  <small className="device-id">{d.id}</small>
                  {authed && i?.state === "ok" && i.status.last_seen_at && <small>最終通信：{formatDateTime(i.status.last_seen_at)}</small>}
                  {i?.state === "error" && <small className="device-row-error" role="alert">{i.message}</small>}
                </div>
                {statusPill(d.id)}
                <div className="device-actions">
                  {authed && !isActive && <button className="button button-outline" onClick={() => setActiveDevice(d.id)}>このデバイスを使う</button>}
                  {!authed && <button className="button button-outline" onClick={() => { setReauth(reauth === d.id ? null : d.id); setReauthPassword(""); setError(""); }}><LockKeyhole size={14} />再ログイン</button>}
                  <button className="text-button device-icon-button" onClick={() => { setEditing(d.id); setDraft(d.name); }} title="表示名を変更" aria-label={`${d.name}の表示名を変更`}><Pencil size={15} /></button>
                  <button className="text-button device-icon-button" onClick={() => remove(d.id, d.name)} title="一覧から外す" aria-label={`${d.name}を一覧から外す`}><Trash2 size={15} /></button>
                </div>
                {reauth === d.id && !authed && <form className="device-reauth" onSubmit={e => void reauthSubmit(e, d.id)}>
                  <p className="device-reauth-hint muted">登録時に設定したパスワードを入力してください。</p>
                  <div className="input-wrap"><LockKeyhole size={16} /><input type="password" autoFocus autoComplete="current-password" placeholder="デバイスのパスワード" value={reauthPassword} onChange={e => setReauthPassword(e.target.value)} aria-label="デバイスのパスワード" required /></div>
                  <button className="button button-primary" type="submit" disabled={busy || !reauthPassword}>{busy ? "認証しています…" : "再ログインする"}</button>
                </form>}
              </div>;
            })}
          </section>}
      </section>
    </section>

    <section id="device-register-panel" className="device-tab-panel" role="tabpanel" aria-labelledby="device-register-tab" hidden={tab !== "register"}>
      <div className="device-panel-intro">
        <span className="device-panel-icon"><Plus size={20} /></span>
        <div><h2>新しいデバイスを登録</h2><p>デバイス本体に設定する新しいIDとパスワードを作成します。登録後はこのアカウントで使えるようになります。</p></div>
      </div>
      <form className="settings-card device-add-form" onSubmit={e => void addDeviceSubmit(e)}>
        <label className="sender-name-field" htmlFor="new-device-id">新しいデバイスID
          <input id="new-device-id" value={newId} onChange={e => setNewId(e.target.value)} placeholder="例：entrance-2" autoComplete="off" autoCapitalize="none" autoCorrect="off" spellCheck={false} minLength={1} maxLength={64} pattern="[A-Za-z0-9_-]+" aria-describedby="new-device-id-hint" required />
          <small id="new-device-id-hint" className="device-field-help">英数字、ハイフン、アンダースコアを使用できます（64文字以内）。</small>
        </label>
        <label className="sender-name-field" htmlFor="new-device-password">新しいパスワード
          <input id="new-device-password" type="password" value={newPassword} onChange={e => setNewPassword(e.target.value)} placeholder="8文字以上" autoComplete="new-password" minLength={8} aria-describedby="new-device-password-hint" required />
          <small id="new-device-password-hint" className="device-field-help">8文字以上で設定してください。</small>
        </label>
        <label className="sender-name-field" htmlFor="new-device-password-confirmation">パスワード（確認）
        <input
          id="new-device-password-confirmation"
          type="password"
          value={newPasswordConfirmation}
          onChange={e => setNewPasswordConfirmation(e.target.value)}
          placeholder="同じパスワードを再入力"
          autoComplete="new-password"
          minLength={8}
          aria-invalid={newPasswordConfirmation.length > 0 && newPasswordConfirmation !== newPassword}
          aria-describedby="new-device-password-confirmation-hint"
          required
        />
        <small id="new-device-password-confirmation-hint" className={`device-field-help ${newPasswordConfirmation ? newPasswordConfirmation === newPassword ? "is-valid" : "is-error" : ""}`}>
          {newPasswordConfirmation && newPasswordConfirmation === newPassword
            ? "パスワードが一致しました。"
            : newPasswordConfirmation
              ? "パスワードが一致しません。"
              : "確認のため、同じパスワードをもう一度入力してください。"}
        </small>
        </label>
        <label className="sender-name-field" htmlFor="new-device-name">表示名（任意）
          <input id="new-device-name" value={newName} maxLength={30} onChange={e => setNewName(e.target.value)} placeholder="例：実家の玄関" />
        </label>
        <button className="button button-primary device-submit" type="submit" disabled={busy || !newId.trim() || newPassword.length < 8 || newPasswordConfirmation !== newPassword}><Plus size={16} />{busy ? "登録しています…" : "新しいデバイスを登録"}</button>
      </form>
      <div className="privacy-note"><KeyRound size={16} /><span>登録したデバイスは現在のアカウントに紐づきます。パスワードは安全に保管されます。</span></div>
    </section>
  </div>;
}
