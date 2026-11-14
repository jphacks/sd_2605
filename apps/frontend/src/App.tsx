import { Navigate, Route, Routes, useNavigate } from "react-router-dom";
import Layout from "./components/Layout";
import HomePage from "./pages/HomePage";
import HistoryPage from "./pages/HistoryPage";
import VoicePage from "./pages/VoicePage";
import SettingsPage from "./pages/SettingsPage";
import FacePage from "./pages/FacePage";
import DevicesPage from "./pages/DevicesPage";
import LoginPage from "./pages/LoginPage";
import InvitePage from "./pages/InvitePage";
import ReceivePage from "./pages/ReceivePage";
import { clearAllSessions, useDevices } from "./devices";

export default function App() {
  // アカウントにログインしていれば認証済み。セッションが切れるとログイン画面へ戻る
  const { authenticated, devices, activeRole } = useDevices();
  const navigate = useNavigate();
  const home = devices.length ? "/" : "/devices";
  const handleLogout = () => { clearAllSessions(); navigate("/login", { replace: true }); };
  return <Routes>
    <Route path="/login" element={authenticated ? <Navigate to={home} replace /> : <LoginPage onLogin={() => navigate(home, { replace: true })} />} />
    <Route element={authenticated ? <Layout onLogout={handleLogout} /> : <Navigate to="/login" replace />}>
      <Route path="/" element={<HomePage />} />
      <Route path="/history" element={<HistoryPage />} />
      <Route path="/voice" element={<VoicePage />} />
      <Route path="/settings" element={<SettingsPage />} />
      <Route path="/face" element={<FacePage />} />
      <Route path="/devices" element={<DevicesPage />} />
      {/* 招待コードの発行は居住者、受け取りは差し出し手の画面。受け取りは、まだ家がないアカウントにも必要 */}
      <Route path="/invite" element={activeRole === "messenger" ? <Navigate to="/receive" replace /> : <InvitePage />} />
      <Route path="/receive" element={<ReceivePage />} />
    </Route>
    <Route path="*" element={<Navigate to={authenticated ? home : "/login"} replace />} />
  </Routes>;
}
