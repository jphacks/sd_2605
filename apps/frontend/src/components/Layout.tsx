import { useEffect, useRef, useState } from "react";
import type { MouseEvent, ReactNode } from "react";
import { NavLink, Navigate, Outlet, useLocation, useNavigate } from "react-router-dom";
import { House, History, Mic, Settings2, LogOut, Cpu } from "lucide-react";
import Robot from "./Robot";
import DeviceSwitcher from "./DeviceSwitcher";
import { getDeviceName, useDevices } from "../devices";

const LOCKED_MENU_MESSAGE = "デバイスが登録されていないため、このメニューには移動できません。";

function MenuLink({ to, end = false, locked, onLocked, children }: { to: string; end?: boolean; locked: boolean; onLocked: (event: MouseEvent<HTMLAnchorElement>) => void; children: ReactNode }) {
  return <NavLink to={to} end={end} className={locked ? "is-locked" : undefined} aria-disabled={locked || undefined} aria-describedby={locked ? "locked-menu-notice" : undefined} title={locked ? "デバイスを登録すると開けます" : undefined} onClick={locked ? onLocked : undefined}>{children}</NavLink>;
}

export default function Layout({ onLogout }: { onLogout: () => void }) {
  const navigate = useNavigate();
  const location = useLocation();
  const { activeId, devices } = useDevices();
  const [notice, setNotice] = useState("");
  const lockNoticeRef = useRef<HTMLDivElement>(null);
  const needsDevice = devices.length === 0;
  const showLockedMenu = (event: MouseEvent<HTMLAnchorElement>) => {
    event.preventDefault();
    lockNoticeRef.current?.focus();
  };
  useEffect(() => {
    const onExpired = (event: Event) => {
      const id = (event as CustomEvent<{ deviceId?: string }>).detail?.deviceId;
      setNotice(`「${id ? getDeviceName(id) : "デバイス"}」のログインが切れました。デバイス管理から再ログインしてください。`);
    };
    window.addEventListener("tadaima:session-expired", onExpired);
    return () => window.removeEventListener("tadaima:session-expired", onExpired);
  }, []);
  // デバイスを切り替えたら画面を作り直し、前のデバイスの表示・録音データが残らないようにする（デバイス管理画面は除く）
  const outletKey = location.pathname === "/devices" ? "devices" : activeId;
  return <div className="app-shell">
    <aside className="sidebar">
      <button className="brand" onClick={() => navigate(needsDevice ? "/devices" : "/")} aria-label={needsDevice ? "デバイス管理へ" : "ホームへ"}><span className="brand-mark"><Robot small /></span><span><strong>おかえりんく</strong><small>おうちのロボット</small></span></button>
      <DeviceSwitcher />
      <div className="sidebar-caption">MENU</div>
      <nav className="side-nav" aria-label="メインメニュー">
        <MenuLink to="/" end locked={needsDevice} onLocked={showLockedMenu}><House size={18} />ホーム</MenuLink>
        <MenuLink to="/history" locked={needsDevice} onLocked={showLockedMenu}><History size={18} />履歴</MenuLink>
        <MenuLink to="/voice" locked={needsDevice} onLocked={showLockedMenu}><Mic size={18} />メッセージ登録</MenuLink>
        <MenuLink to="/settings" locked={needsDevice} onLocked={showLockedMenu}><Settings2 size={18} />デバイス情報</MenuLink>
        <NavLink to="/devices"><Cpu size={18} />デバイス管理</NavLink>
      </nav>
      {needsDevice && <p className="nav-lock-note">デバイス未登録のため、ほかのメニューはまだ開けません。</p>}
      <button className="logout-button" onClick={onLogout}><LogOut size={16} />すべてログアウト</button>
    </aside>
    <main className="main-area">
      <header className="mobile-header"><span className="brand-dot"><Robot small /></span><strong>おかえりんく</strong><DeviceSwitcher compact /><button className="mobile-logout" onClick={onLogout} aria-label="すべてログアウト"><LogOut size={17} /></button></header>
      {needsDevice && <div className="notice-wrap"><div id="locked-menu-notice" className="inline-alert nav-lock-banner" role="status" tabIndex={-1} ref={lockNoticeRef}>{LOCKED_MENU_MESSAGE}</div></div>}
      {notice && <div className="notice-wrap"><div className="inline-alert error" role="alert">{notice}<button className="text-button" onClick={() => setNotice("")}>閉じる</button></div></div>}
      {needsDevice && location.pathname !== "/devices" ? <Navigate to="/devices" replace /> : <Outlet key={outletKey} />}
    </main>
    <nav className="mobile-nav" aria-label="メインメニュー">
      <MenuLink to="/" end locked={needsDevice} onLocked={showLockedMenu}><House size={19} /><span>ホーム</span></MenuLink>
      <MenuLink to="/history" locked={needsDevice} onLocked={showLockedMenu}><History size={19} /><span>履歴</span></MenuLink>
      <MenuLink to="/voice" locked={needsDevice} onLocked={showLockedMenu}><Mic size={19} /><span>メッセージ</span></MenuLink>
      <MenuLink to="/settings" locked={needsDevice} onLocked={showLockedMenu}><Settings2 size={19} /><span>情報</span></MenuLink>
    </nav>
  </div>;
}
