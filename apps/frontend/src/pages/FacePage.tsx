import { useEffect, useRef, useState } from "react";
import type { ChangeEvent } from "react";
import { Link } from "react-router-dom";
import { Save, Smile, Upload } from "lucide-react";
import { ApiError, apiRequest, devicePath } from "../api";
import { CATALOG, OPTION_LABELS, PRESETS, SLOT_LABELS, cloneCharacter, optionLabel, type Character } from "../character/catalog";
import { renderFace } from "../character/drawFace";
import { useDevices } from "../devices";

const PHOTO_TYPES = ["image/jpeg", "image/png", "image/webp"];
const SLOTS = Object.keys(SLOT_LABELS) as Array<keyof typeof SLOT_LABELS>;

const sameFace = (a: Character, b: Character) => JSON.stringify(cloneCharacter(a)) === JSON.stringify(cloneCharacter(b));

export default function FacePage() {
  const { active, activeRole } = useDevices();
  const readOnly = activeRole === "messenger";
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [draft, setDraft] = useState<Character>(() => cloneCharacter(PRESETS.boy));
  const [saved, setSaved] = useState<Character | null>(null);
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState("");
  const [error, setError] = useState("");

  const refresh = async () => {
    setLoading(true);
    setError("");
    setMessage("");
    try {
      const character = await apiRequest<Character>(devicePath("/character"));
      const next = cloneCharacter({ ...PRESETS.boy, ...character, extras: character.extras ?? [] });
      setSaved(next);
      setDraft(cloneCharacter(next));
    } catch (e) {
      if (e instanceof ApiError && e.status === 404) {
        const preset = cloneCharacter(PRESETS.boy);
        setSaved(null);
        setDraft(preset);
      } else {
        setError(e instanceof Error ? e.message : "顔を読み込めませんでした。");
      }
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { void refresh(); }, [active?.id]);
  useEffect(() => {
    const canvas = canvasRef.current;
    if (canvas) renderFace(canvas, draft);
  }, [draft]);

  const edit = (slot: keyof Character, value: string) => {
    setDraft((current) => ({
      ...current,
      [slot]: value,
      uncertain: (current.uncertain ?? []).filter((name) => name !== slot),
    }));
    setMessage("");
  };

  const toggleExtra = (id: string) => {
    setDraft((current) => {
      const extras = current.extras.includes(id) ? current.extras.filter((item) => item !== id) : [...current.extras, id];
      return { ...current, extras, uncertain: (current.uncertain ?? []).filter((name) => name !== "extras") };
    });
    setMessage("");
  };

  const extract = async (event: ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    if (!PHOTO_TYPES.includes(file.type) || file.size > 4 * 1024 * 1024) {
      setError("写真は JPEG、PNG、WebP で、4MB 以下にしてください。");
      return;
    }
    setBusy(true);
    setError("");
    setMessage("");
    try {
      const body = new FormData();
      body.append("file", file);
      const suggestion = await apiRequest<Character>(devicePath("/character/extract"), { method: "POST", body });
      setDraft(cloneCharacter({ ...PRESETS.boy, ...suggestion, extras: suggestion.extras ?? [] }));
      setMessage("写真から顔を選びました。保存するまで、この家の顔は変わりません。");
    } catch (e) {
      setError(e instanceof Error ? e.message : "写真から顔を作れませんでした。");
    } finally {
      setBusy(false);
    }
  };

  const save = async () => {
    setBusy(true);
    setError("");
    setMessage("");
    try {
      const stored = await apiRequest<Character>(devicePath("/character"), { method: "PUT", body: JSON.stringify(cloneCharacter(draft)) });
      const next = cloneCharacter({ ...PRESETS.boy, ...stored, extras: stored.extras ?? [] });
      setSaved(next);
      setDraft(cloneCharacter(next));
      setMessage("この家の顔を保存しました。玄関のデバイスは、次に顔を取りにいったときからこの版を使えます。");
    } catch (e) {
      setError(e instanceof Error ? e.message : "顔を保存できませんでした。");
    } finally {
      setBusy(false);
    }
  };

  const uncertain = new Set(draft.uncertain ?? []);
  const dirty = !saved || !sameFace(draft, saved);

  return <div className="page-content">
    <div className="page-heading">
      <div>
        <p className="eyebrow">ROBOT FACE</p>
        <h1>ロボットの顔</h1>
        <p className="muted">{active?.name ?? "この家"} の顔を、写真かパーツから作ります。</p>
      </div>
      <div className="heading-icon"><Smile size={21} /></div>
    </div>
    {error && <div className="inline-alert error">{error}</div>}
    {message && <div className="inline-alert success">{message}</div>}
    {readOnly && <div className="inline-alert info" role="status">あなたはこの家の差し出し手です。顔の確認はできますが、変更できるのは居住者だけです。</div>}
    {!saved && !loading && !error && <div className="inline-alert info" role="status">まだ保存されていません。表示しているのは見本の顔です。</div>}

    <section className="settings-card face-card">
      <div className="face-layout">
        <canvas ref={canvasRef} className="face-stage" width={320} height={240} aria-label="ロボットの顔のプレビュー" />
        <div className="face-editor">
          {!readOnly && <>
            <div className="face-presets" role="group" aria-label="見本の顔">
              {(Object.keys(PRESETS) as Array<keyof typeof PRESETS>).map((id) => (
                <button key={id} type="button" className="button button-outline" disabled={loading || busy} onClick={() => { setDraft(cloneCharacter(PRESETS[id])); setMessage(""); }}>
                  {id === "boy" ? "男の子" : id === "girl" ? "女の子" : "友だち"}
                </button>
              ))}
            </div>
            <label className="button button-outline face-upload">
              <Upload size={15} />{busy ? "作成中…" : "写真から作る"}
              <input type="file" accept={PHOTO_TYPES.join(",")} onChange={(event) => void extract(event)} disabled={loading || busy} />
            </label>
            <div className="face-controls">
              {SLOTS.map((slot) => (
                <label className="face-field" key={slot}>
                  <span>{SLOT_LABELS[slot]}{uncertain.has(slot) ? "（自信なし）" : ""}</span>
                  <select value={draft[slot]} disabled={loading || busy} onChange={(event) => edit(slot, event.target.value)}>
                    {CATALOG[slot].map((id) => <option key={id} value={id}>{optionLabel(id)}</option>)}
                  </select>
                </label>
              ))}
            </div>
            <fieldset className="face-extras" disabled={loading || busy}>
              <legend>飾り{uncertain.has("extras") ? "（自信なし）" : ""}</legend>
              {CATALOG.extras.map((id) => (
                <label key={id}>
                  <input type="checkbox" checked={draft.extras.includes(id)} onChange={() => toggleExtra(id)} />
                  {OPTION_LABELS[id]}
                </label>
              ))}
            </fieldset>
            <div className="settings-footer">
              <span className="muted">{dirty ? "保存すると、この家の顔として残ります。" : "保存済みの顔です。"}</span>
              <button className="button button-primary" type="button" onClick={() => void save()} disabled={loading || busy || !dirty}><Save size={16} />{busy ? "保存中…" : "顔を保存"}</button>
            </div>
          </>}
          {readOnly && !loading && !error && <p className="muted">{saved ? "保存されている顔です。" : "居住者が保存すると、ここに表示されます。"}</p>}
        </div>
      </div>
    </section>
    <Link className="text-button" to="/settings">デバイス情報へ戻る</Link>
  </div>;
}
