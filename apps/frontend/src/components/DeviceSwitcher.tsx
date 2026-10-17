import { Link, useNavigate } from "react-router-dom";
import { Settings2 } from "lucide-react";
import { setActiveDevice, useDevices } from "../devices";

/** 操作するデバイスの切り替え。ログインしていないデバイスを選ぶと、デバイス管理で再ログインできる */
export default function DeviceSwitcher({ compact = false }: { compact?: boolean }) {
  const { devices, activeId, hasSession } = useDevices();
  const navigate = useNavigate();
  const change = (id: string) => { if (!setActiveDevice(id)) navigate("/devices"); };
  return <div className={`device-switcher ${compact ? "is-compact" : ""}`}>
    {!compact && <label htmlFor="device-select">操作中のロボット</label>}
    <div className="device-select-row">
      <select id="device-select" aria-label="操作中のロボット" value={activeId} onChange={e => change(e.target.value)}>
        {devices.map(d => <option key={d.id} value={d.id}>{d.name}{hasSession(d.id) ? "" : "（要ログイン）"}</option>)}
      </select>
      {compact && <Link to="/devices" className="device-manage-link" aria-label="デバイス管理"><Settings2 size={17} /></Link>}
    </div>
  </div>;
}
