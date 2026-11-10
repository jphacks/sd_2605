import { useRef, useState } from "react";
import { Camera, RotateCcw } from "lucide-react";
import DeviceIcon, { DEFAULT_ICON_ID, PRESET_ICONS } from "./DeviceIcon";
import { photoToIconDataUrl } from "../icons";
import type { DeviceIcon as DeviceIconValue } from "../devices";

/** デフォルトアイコンの選択と、写真のアップロード。value が undefined のときは標準（ロボット） */
export default function IconPicker({ value, onChange, disabled = false }: { value?: DeviceIconValue; onChange: (icon: DeviceIconValue | undefined) => void; disabled?: boolean }) {
  const fileRef = useRef<HTMLInputElement>(null);
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  const selectedId = value?.kind === "preset" ? value.id : value ? "" : DEFAULT_ICON_ID;
  const pickFile = async (file?: File) => {
    if (!file) return;
    setError(""); setBusy(true);
    try { onChange({ kind: "photo", dataUrl: await photoToIconDataUrl(file) }); }
    catch (e) { setError(e instanceof Error ? e.message : "画像を設定できませんでした。"); }
    finally { setBusy(false); if (fileRef.current) fileRef.current.value = ""; }
  };
  return <div className="icon-picker">
    <div className="icon-picker-current"><DeviceIcon icon={value} size={64} label="現在のアイコン" /><div><strong>{value?.kind === "photo" ? "アップロードした写真" : "標準のアイコン"}</strong><small>アイコンはこのブラウザにだけ保存され、ほかの端末には共有されません。</small></div></div>
    <div className="icon-grid" role="radiogroup" aria-label="アイコンを選ぶ">
      {PRESET_ICONS.map(p => <button type="button" key={p.id} role="radio" aria-checked={selectedId === p.id} className={selectedId === p.id ? "is-selected" : ""} onClick={() => onChange(p.id === DEFAULT_ICON_ID ? undefined : { kind: "preset", id: p.id })} disabled={disabled || busy} title={p.label}><DeviceIcon icon={{ kind: "preset", id: p.id }} size={40} /><span>{p.label}</span></button>)}
    </div>
    <div className="icon-picker-actions">
      <input ref={fileRef} type="file" accept="image/*" hidden onChange={e => void pickFile(e.target.files?.[0])} aria-label="アイコンの写真を選ぶ" />
      <button type="button" className="button button-outline" onClick={() => fileRef.current?.click()} disabled={disabled || busy}><Camera size={15} />{busy ? "変換中…" : "写真をアップロード"}</button>
      {value && <button type="button" className="button button-quiet" onClick={() => { setError(""); onChange(undefined); }} disabled={disabled || busy}><RotateCcw size={14} />標準に戻す</button>}
    </div>
    {error && <div className="inline-alert error" role="alert">{error}</div>}
  </div>;
}
