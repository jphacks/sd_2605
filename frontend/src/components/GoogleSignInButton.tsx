import { useEffect, useRef, useState } from "react";
import { getAuthConfig } from "../api";

type GoogleCredentialResponse = { credential?: string };
type GoogleIdentity = {
  accounts: {
    id: {
      initialize: (config: { client_id: string; callback: (response: GoogleCredentialResponse) => void }) => void;
      renderButton: (parent: HTMLElement, options: { type: "standard"; theme: "outline"; size: "large"; text: "continue_with"; shape: "rectangular"; width: number }) => void;
      cancel: () => void;
    };
  };
};

declare global {
  interface Window { google?: GoogleIdentity }
}

let googleScript: Promise<void> | undefined;

function loadGoogleIdentityScript(): Promise<void> {
  if (window.google?.accounts.id) return Promise.resolve();
  googleScript ??= new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.src = "https://accounts.google.com/gsi/client?hl=ja";
    script.async = true;
    script.onload = () => resolve();
    script.onerror = () => {
      googleScript = undefined;
      reject(new Error("Googleログインを読み込めませんでした。"));
    };
    document.head.appendChild(script);
  });
  return googleScript;
}

export default function GoogleSignInButton({ disabled, onCredential, onError }: {
  disabled: boolean;
  onCredential: (credential: string) => void;
  onError: (message: string) => void;
}) {
  const buttonRef = useRef<HTMLDivElement>(null);
  const onCredentialRef = useRef(onCredential);
  const onErrorRef = useRef(onError);
  const disabledRef = useRef(disabled);
  onCredentialRef.current = onCredential;
  onErrorRef.current = onError;
  disabledRef.current = disabled;
  const [clientId, setClientId] = useState<string | null>(null);
  const [scriptFailed, setScriptFailed] = useState(false);

  useEffect(() => {
    let cancelled = false;
    getAuthConfig()
      .then(config => { if (!cancelled) setClientId(config.google_client_id); })
      .catch(() => { if (!cancelled) setClientId(null); });
    return () => { cancelled = true; };
  }, []);

  useEffect(() => {
    const parent = buttonRef.current;
    if (!clientId || !parent) return;
    let cancelled = false;
    loadGoogleIdentityScript().then(() => {
      if (cancelled || !buttonRef.current || !window.google) return;
      const target = buttonRef.current;
      window.google.accounts.id.initialize({
        client_id: clientId,
        callback: response => {
          if (disabledRef.current || !response.credential) return;
          onCredentialRef.current(response.credential);
        },
      });
      target.replaceChildren();
      window.google.accounts.id.renderButton(target, {
        type: "standard",
        theme: "outline",
        size: "large",
        text: "continue_with",
        shape: "rectangular",
        width: Math.max(Math.min(target.clientWidth, 400), 200),
      });
    }).catch(error => {
      if (cancelled) return;
      setScriptFailed(true);
      onErrorRef.current(error instanceof Error ? error.message : "Googleログインを読み込めませんでした。");
    });
    return () => {
      cancelled = true;
      window.google?.accounts.id.cancel();
    };
  }, [clientId]);

  if (!clientId || scriptFailed) return null;
  return <>
    <div className="login-divider">または</div>
    <div className={`google-signin ${disabled ? "is-busy" : ""}`} ref={buttonRef} aria-label="Googleでログイン" />
  </>;
}
