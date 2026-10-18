import { useEffect, useState } from "react";
import { NavLink, Navigate, Outlet, useLocation, useNavigate } from "react-router-dom";
import { House, History, Mic, Settings2, LogOut, Cpu } from "lucide-react";
import Robot from "./Robot";
import DeviceSwitcher from "./DeviceSwitcher";
import { getDeviceName, useDevices } from "../devices";

export default function Layout({ onLogout }: { onLogout: () => void }) {
  const navigate = useNavigate();
  const location = useLocation();
  const { activeId, devices } = useDevices();
  const [notice, setNotice] = useState("");
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
      <button className="brand" onClick={() => navigate("/")} aria-label="ホームへ"><span className="brand-mark"><Robot small /></span><span><strong>おかえりんく</strong><small>おうちのロボット</small></span></button>
      <DeviceSwitcher />
      <div className="sidebar-caption">MENU</div>
      <nav className="side-nav"><NavLink to="/" end><House size={18} />ホーム</NavLink><NavLink to="/history"><History size={18} />履歴</NavLink><NavLink to="/voice"><Mic size={18} />メッセージ登録</NavLink><NavLink to="/settings"><Settings2 size={18} />デバイス情報</NavLink><NavLink to="/devices"><Cpu size={18} />デバイス管理</NavLink></nav>
      <button className="logout-button" onClick={onLogout}><LogOut size={16} />すべてログアウト</button>
    </aside>
    <main className="main-area">
      <header className="mobile-header"><span className="brand-dot"><Robot small /></span><strong>おかえりんく</strong><DeviceSwitcher compact /><button className="mobile-logout" onClick={onLogout} aria-label="すべてログアウト"><LogOut size={17} /></button></header>
      {notice && <div className="notice-wrap"><div className="inline-alert error" role="alert">{notice}<button className="text-button" onClick={() => setNotice("")}>閉じる</button></div></div>}
      {devices.length === 0 && location.pathname !== "/devices" ? <Navigate to="/devices" replace /> : <Outlet key={outletKey} />}
    </main>
    <nav className="mobile-nav"><NavLink to="/" end><House size={19} /><span>ホーム</span></NavLink><NavLink to="/history"><History size={19} /><span>履歴</span></NavLink><NavLink to="/voice"><Mic size={19} /><span>メッセージ</span></NavLink><NavLink to="/settings"><Settings2 size={19} /><span>情報</span></NavLink></nav>
  </div>;
}
