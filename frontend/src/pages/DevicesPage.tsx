import { useCallback, useEffect, useState } from "react";
import type { FormEvent } from "react";
import { Cpu, KeyRound, LockKeyhole, Pencil, Plus, RefreshCw, Trash2, Check, X } from "lucide-react";
import { apiRequest, devicePath, login, DeviceStatus, formatDateTime } from "../api";
import { DEVICE_ID_PATTERN, removeDevice, renameDevice, setActiveDevice, useDevices } from "../devices";

type Info = { state: "loading" } | { state: "ok"; status: DeviceStatus } | { state: "error" };

export default function DevicesPage() {
  const { devices, activeId, hasSession } = useDevices();
  const [info, setInfo] = useState<Record<string, Info>>({});
  const [error, setError] = useState("");
  const [message, setMessage] = useState("");
  const [busy, setBusy] = useState(false);
  const [newId, setNewId] = useState("");
  const [newName, setNewName] = useState("");
  const [newPassword, setNewPassword] = useState("");
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
      catch { setInfo(prev => ({ ...prev, [id]: { state: "error" } })); }
    }));
  }, [loggedInKey]);
  useEffect(() => { void load(); }, [load]);

  const addDeviceSubmit = async (event: FormEvent) => {
    event.preventDefault(); setError(""); setMessage("");
    const id = newId.trim();
    if (!DEVICE_ID_PATTERN.test(id)) { setError("デバイスIDは英数字・ハイフン・アンダースコアで入力してください。"); return; }
    if (hasSession(id)) { setError("このデバイスはすでに追加されています。"); return; }
    setBusy(true);
    try {
      await login(id, newPassword);                 // 認証に成功すると一覧に追加され、操作中になる
      if (newName.trim()) renameDevice(id, newName);
      setMessage(`「${newName.trim() || id}」を追加しました。このデバイスを操作中です。`);
      setNewId(""); setNewName(""); setNewPassword("");
    } catch (e) { setError(e instanceof Error ? e.message : "ログインできませんでした。"); }
    finally { setBusy(false); }
  };

  const reauthSubmit = async (event: FormEvent, id: string) => {
    event.preventDefault(); setError(""); setMessage(""); setBusy(true);
    try { await login(id, reauthPassword); setReauth(null); setReauthPassword(""); }
    catch (e) { setError(e instanceof Error ? e.message : "ログインできませんでした。"); }
    finally { setBusy(false); }
  };

  const saveName = (id: string) => { renameDevice(id, draft); setEditing(null); };
  const remove = (id: string, name: string) => {
    if (window.confirm(`「${name}」を一覧から外し、ログアウトしますか？\nデバイス本体やサーバー上のデータは削除されません。`)) { removeDevice(id); setMessage(`「${name}」を一覧から外しました。`); }
  };
  const statusPill = (id: string) => {
    if (!hasSession(id)) return <span className="status-pill"><span className="status-dot is-offline" />要ログイン</span>;
    const i = info[id];
    if (!i || i.state === "loading") return <span className="status-pill">確認中</span>;
    if (i.state === "error") return <span className="status-pill"><span className="status-dot is-offline" />取得できません</span>;
    return <span className="status-pill"><span className={`status-dot ${i.status.online ? "" : "is-offline"}`} />{i.status.online ? "オンライン" : "オフライン"}</span>;
  };

  return <div className="page-content">
    <div className="page-heading"><div><p className="eyebrow">MY ROBOTS</p><h1>デバイス管理</h1><p className="muted">複数のロボットを登録して、切り替えながら操作できます。</p></div><button className="button button-outline" onClick={() => void load()}><RefreshCw size={15} />更新</button></div>
    {error && <div className="inline-alert error" role="alert">{error}</div>}
    {message && <div className="inline-alert success" role="status">{message}</div>}

    <section className="activity-card device-list">
      {devices.map(d => {
        const i = info[d.id]; const authed = hasSession(d.id); const isActive = d.id === activeId;
        return <div className={`activity-row device-row ${isActive ? "is-active" : ""}`} key={d.id}>
          <div className="event-symbol"><Cpu size={17} /></div>
          <div className="activity-main">
            {editing === d.id
              ? <div className="device-rename"><input autoFocus aria-label="表示名" value={draft} maxLength={30} onChange={e => setDraft(e.target.value)} onKeyDown={e => { if (e.key === "Enter") saveName(d.id); if (e.key === "Escape") setEditing(null); }} /><button className="text-button" onClick={() => saveName(d.id)} aria-label="名前を保存"><Check size={16} /></button><button className="text-button" onClick={() => setEditing(null)} aria-label="キャンセル"><X size={16} /></button></div>
              : <strong>{d.name}{isActive && <span className="device-badge">操作中</span>}</strong>}
            <small>{d.id}{authed && i?.state === "ok" ? ` ・ 最終通信 ${formatDateTime(i.status.last_seen_at)}` : ""}</small>
          </div>
          {statusPill(d.id)}
          <div className="device-actions">
            {authed && !isActive && <button className="button button-outline" onClick={() => setActiveDevice(d.id)}>操作する</button>}
            {!authed && <button className="button button-outline" onClick={() => { setReauth(reauth === d.id ? null : d.id); setReauthPassword(""); setError(""); }}><LockKeyhole size={14} />ログイン</button>}
            <button className="text-button" onClick={() => { setEditing(d.id); setDraft(d.name); }} aria-label={`${d.name}の表示名を変更`}><Pencil size={15} /></button>
            <button className="text-button" onClick={() => remove(d.id, d.name)} disabled={devices.length <= 1} aria-label={`${d.name}を一覧から外す`}><Trash2 size={15} /></button>
          </div>
          {reauth === d.id && !authed && <form className="device-reauth" onSubmit={e => void reauthSubmit(e, d.id)}>
            <div className="input-wrap"><LockKeyhole size={16} /><input type="password" autoFocus autoComplete="current-password" placeholder="パスワードを入力" value={reauthPassword} onChange={e => setReauthPassword(e.target.value)} required /></div>
            <button className="button button-primary" type="submit" disabled={busy || !reauthPassword}>{busy ? "確認中…" : "ログインして切り替え"}</button>
          </form>}
        </div>;
      })}
    </section>

    <div className="section-heading"><div><h2>デバイスを追加</h2><p className="muted">家主から共有されたデバイスIDとパスワードでログインします。</p></div></div>
    <form className="settings-card device-add-form" onSubmit={e => void addDeviceSubmit(e)}>
      <label className="sender-name-field">デバイスID<input value={newId} onChange={e => setNewId(e.target.value)} placeholder="例：entrance-2" autoComplete="username" required /></label>
      <label className="sender-name-field">パスワード<input type="password" value={newPassword} onChange={e => setNewPassword(e.target.value)} placeholder="パスワードを入力" autoComplete="new-password" required /></label>
      <label className="sender-name-field">表示名（任意）<input value={newName} maxLength={30} onChange={e => setNewName(e.target.value)} placeholder="例：実家の玄関" /></label>
      <button className="button button-primary" type="submit" disabled={busy || !newId.trim() || !newPassword}><Plus size={16} />{busy ? "確認中…" : "追加してログイン"}</button>
    </form>
    <div className="privacy-note"><KeyRound size={16} /><span>ログイン情報はこのブラウザを閉じると消えます。デバイスごとに別のパスワードで認証されます。</span></div>
  </div>;
}
