/** 家のアイコン用の写真を、正方形に切り抜いて小さな JPEG（data URL）にする。サーバーには送らない。 */
export const ICON_SIZE = 160;
export const MAX_PHOTO_BYTES = 8 * 1024 * 1024;

export async function photoToIconDataUrl(file: File): Promise<string> {
  if (!file.type.startsWith("image/")) throw new Error("画像ファイルを選んでください。");
  if (file.size > MAX_PHOTO_BYTES) throw new Error("画像は8MB以下のものを選んでください。");
  const url = URL.createObjectURL(file);
  try {
    const image = await new Promise<HTMLImageElement>((resolve, reject) => {
      const el = new Image();
      el.onload = () => resolve(el);
      el.onerror = () => reject(new Error("この画像は読み込めませんでした。JPEG・PNG・WebPなどを選んでください。"));
      el.src = url;
    });
    const side = Math.min(image.naturalWidth, image.naturalHeight);
    if (!side) throw new Error("この画像は読み込めませんでした。");
    const canvas = document.createElement("canvas");
    canvas.width = canvas.height = ICON_SIZE;
    const ctx = canvas.getContext("2d");
    if (!ctx) throw new Error("このブラウザでは画像を変換できません。");
    ctx.fillStyle = "#fffefa"; // 透過PNGの背景
    ctx.fillRect(0, 0, ICON_SIZE, ICON_SIZE);
    ctx.drawImage(image, (image.naturalWidth - side) / 2, (image.naturalHeight - side) / 2, side, side, 0, 0, ICON_SIZE, ICON_SIZE);
    return canvas.toDataURL("image/jpeg", 0.85);
  } finally { URL.revokeObjectURL(url); }
}
