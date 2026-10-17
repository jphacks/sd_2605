import { Navigate, Route, Routes, useNavigate } from "react-router-dom";
import Layout from "./components/Layout";
import HomePage from "./pages/HomePage";
import HistoryPage from "./pages/HistoryPage";
import VoicePage from "./pages/VoicePage";
import SettingsPage from "./pages/SettingsPage";
import DevicesPage from "./pages/DevicesPage";
import LoginPage from "./pages/LoginPage";
import { clearAllSessions, useDevices } from "./devices";

export default function App() {
  // どれか1台でもログインしていれば認証済み。全デバイスのセッションが切れると自動でログイン画面へ戻る
  const { authenticated } = useDevices();
  const navigate = useNavigate();
  const handleLogout = () => { clearAllSessions(); navigate("/login", { replace: true }); };
  return <Routes>
    <Route path="/login" element={authenticated ? <Navigate to="/" replace /> : <LoginPage onLogin={() => navigate("/", { replace: true })} />} />
    <Route element={authenticated ? <Layout onLogout={handleLogout} /> : <Navigate to="/login" replace />}>
      <Route path="/" element={<HomePage />} />
      <Route path="/history" element={<HistoryPage />} />
      <Route path="/voice" element={<VoicePage />} />
      <Route path="/settings" element={<SettingsPage />} />
      <Route path="/devices" element={<DevicesPage />} />
    </Route>
    <Route path="*" element={<Navigate to={authenticated ? "/" : "/login"} replace />} />
  </Routes>;
}
