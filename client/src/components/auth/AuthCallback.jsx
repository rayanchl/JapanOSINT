import React, { useEffect, useRef, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../../auth/AuthContext.jsx';
import { sessionStore } from '../../auth/session.js';
import { Button, ErrorNotice, Spinner } from '../ui/kit.jsx';

/** `/auth/callback` — completes the PKCE exchange after a social sign-in. */
export default function AuthCallback() {
  const auth = useAuth();
  const navigate = useNavigate();
  const [error, setError] = useState(null);
  const ran = useRef(false);

  useEffect(() => {
    if (ran.current) return;
    ran.current = true;
    (async () => {
      try {
        await auth.completeOAuthCallback(window.location.href);
        const back = sessionStore.oauthReturn;
        sessionStore.setOauthReturn(null);
        if (sessionStore.onboardingCompleted) auth.completeOnboarding();
        navigate(back && !back.startsWith('/auth/') ? back : '/', { replace: true });
      } catch (e) {
        setError(e);
      }
    })();
  }, [auth, navigate]);

  return (
    <div className="h-screen w-screen flex items-center justify-center bg-osint-bg text-osint-text p-6">
      <div className="w-full max-w-md space-y-3 text-center">
        {!error ? (
          <>
            <Spinner size={22} className="mx-auto" />
            <div className="text-sm text-osint-muted">Completing sign-in…</div>
          </>
        ) : (
          <>
            <ErrorNotice error={error} title="Social sign-in failed" />
            <Button onClick={() => navigate('/', { replace: true })}>Back to sign-in</Button>
          </>
        )}
      </div>
    </div>
  );
}
