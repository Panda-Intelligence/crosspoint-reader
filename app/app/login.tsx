import React, { useEffect } from 'react';
import {
  Alert,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import { useRouter } from 'expo-router';
import * as WebBrowser from 'expo-web-browser';
import * as Google from 'expo-auth-session/providers/google';
import * as AuthSession from 'expo-auth-session';

import { useAuth, User } from '../src/contexts/AuthContext';
import { Palette, Spacing, Radii } from '../src/ui-theme';

WebBrowser.maybeCompleteAuthSession();

// OAuth client IDs are read from EXPO_PUBLIC_* env vars at build time
// (see app/.env.example). When unset, the corresponding sign-in option
// is rendered disabled with a "Not configured" hint, instead of going
// through OAuth and failing with an unhelpful HTTP 400.
const GOOGLE_IOS_CLIENT_ID = process.env.EXPO_PUBLIC_GOOGLE_IOS_CLIENT_ID ?? '';
const GOOGLE_ANDROID_CLIENT_ID = process.env.EXPO_PUBLIC_GOOGLE_ANDROID_CLIENT_ID ?? '';
const GOOGLE_WEB_CLIENT_ID = process.env.EXPO_PUBLIC_GOOGLE_WEB_CLIENT_ID ?? '';
const GITHUB_CLIENT_ID = process.env.EXPO_PUBLIC_GITHUB_CLIENT_ID ?? '';

// At least one of the platform-specific Google IDs must be set for the
// Google flow to be usable. Web ID is always required.
const GOOGLE_CONFIGURED =
  GOOGLE_WEB_CLIENT_ID !== '' &&
  (GOOGLE_IOS_CLIENT_ID !== '' || GOOGLE_ANDROID_CLIENT_ID !== '');
const GITHUB_CONFIGURED = GITHUB_CLIENT_ID !== '';

const GITHUB_DISCOVERY: AuthSession.DiscoveryDocument = {
  authorizationEndpoint: 'https://github.com/login/oauth/authorize',
  tokenEndpoint: 'https://github.com/login/oauth/access_token',
  revocationEndpoint: GITHUB_CLIENT_ID
    ? `https://github.com/settings/connections/applications/${GITHUB_CLIENT_ID}`
    : 'https://github.com/settings/applications',
};

export default function LoginScreen() {
  const { login } = useAuth();
  const router = useRouter();

  // --- Google ---
  const [googleRequest, googleResponse, googlePromptAsync] = Google.useAuthRequest({
    iosClientId: GOOGLE_IOS_CLIENT_ID || undefined,
    androidClientId: GOOGLE_ANDROID_CLIENT_ID || undefined,
    webClientId: GOOGLE_WEB_CLIENT_ID || undefined,
  });

  useEffect(() => {
    if (googleResponse?.type === 'success') {
      void fetchGoogleUser(googleResponse.authentication?.accessToken);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [googleResponse]);

  const fetchGoogleUser = async (token?: string) => {
    if (!token) return;
    try {
      const res = await fetch('https://www.googleapis.com/userinfo/v2/me', {
        headers: { Authorization: `Bearer ${token}` },
      });
      const u = await res.json();
      const userData: User = {
        id: u.id,
        name: u.name,
        email: u.email,
        photo: u.picture,
        provider: 'google',
      };
      await login(userData);
      router.replace('/(tabs)');
    } catch (e) {
      console.error('Failed to fetch Google user', e);
      Alert.alert('Sign-in failed', 'Could not fetch Google profile.');
    }
  };

  // --- GitHub (PKCE, no client secret) ---
  const githubRedirectUri = AuthSession.makeRedirectUri({
    scheme: 'murphymate',
    path: 'auth/github',
  });

  const [githubRequest, githubResponse, githubPromptAsync] =
    AuthSession.useAuthRequest(
      {
        clientId: GITHUB_CLIENT_ID || 'unset',
        scopes: ['read:user', 'user:email'],
        redirectUri: githubRedirectUri,
        usePKCE: true,
      },
      GITHUB_DISCOVERY,
    );

  useEffect(() => {
    if (githubResponse?.type === 'success' && githubRequest?.codeVerifier) {
      void completeGithub(
        githubResponse.params.code,
        githubRequest.codeVerifier,
      );
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [githubResponse]);

  const completeGithub = async (code: string, codeVerifier: string) => {
    try {
      const tokenResponse = await AuthSession.exchangeCodeAsync(
        {
          clientId: GITHUB_CLIENT_ID,
          code,
          redirectUri: githubRedirectUri,
          extraParams: { code_verifier: codeVerifier },
        },
        { tokenEndpoint: GITHUB_DISCOVERY.tokenEndpoint! },
      );
      const accessToken = tokenResponse.accessToken;
      if (!accessToken) throw new Error('No access token in response');

      const userRes = await fetch('https://api.github.com/user', {
        headers: {
          Authorization: `Bearer ${accessToken}`,
          Accept: 'application/vnd.github+json',
        },
      });
      if (!userRes.ok) throw new Error(`GitHub /user ${userRes.status}`);
      const profile = await userRes.json();

      let email: string | undefined = profile.email;
      if (!email) {
        try {
          const emailRes = await fetch('https://api.github.com/user/emails', {
            headers: {
              Authorization: `Bearer ${accessToken}`,
              Accept: 'application/vnd.github+json',
            },
          });
          if (emailRes.ok) {
            const emails: Array<{ email: string; primary: boolean; verified: boolean }> =
              await emailRes.json();
            email = emails.find((e) => e.primary && e.verified)?.email
              ?? emails.find((e) => e.verified)?.email;
          }
        } catch {
          // optional — fall back to placeholder below
        }
      }

      const userData: User = {
        id: String(profile.id),
        name: profile.name || profile.login,
        email: email || `${profile.login}@users.noreply.github.com`,
        photo: profile.avatar_url,
        provider: 'github',
      };
      await login(userData);
      router.replace('/(tabs)');
    } catch (e: any) {
      console.error('GitHub sign-in failed', e);
      Alert.alert('Sign-in failed', e?.message ?? 'GitHub OAuth error.');
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Murphy Mate</Text>
      <Text style={styles.subtitle}>Sign in to manage your Crosspoint Reader</Text>

      <TouchableOpacity
        style={[styles.googleButton, !GOOGLE_CONFIGURED && styles.buttonDisabled]}
        disabled={!GOOGLE_CONFIGURED || !googleRequest}
        onPress={() => googlePromptAsync()}
        accessibilityRole="button"
        accessibilityLabel="Sign in with Google"
        accessibilityState={{ disabled: !GOOGLE_CONFIGURED }}>
        <Text style={styles.googleButtonText}>Sign in with Google</Text>
        {!GOOGLE_CONFIGURED && (
          <Text style={styles.notConfigured}>Not configured</Text>
        )}
      </TouchableOpacity>

      <TouchableOpacity
        style={[styles.githubButton, !GITHUB_CONFIGURED && styles.buttonDisabled]}
        disabled={!GITHUB_CONFIGURED || !githubRequest}
        onPress={() => githubPromptAsync()}
        accessibilityRole="button"
        accessibilityLabel="Sign in with GitHub"
        accessibilityState={{ disabled: !GITHUB_CONFIGURED }}>
        <Text style={styles.githubButtonText}>Sign in with GitHub</Text>
        {!GITHUB_CONFIGURED && (
          <Text style={styles.notConfigured}>Not configured</Text>
        )}
      </TouchableOpacity>

      {(!GOOGLE_CONFIGURED || !GITHUB_CONFIGURED) && (
        <Text style={styles.helpText}>
          Set EXPO_PUBLIC_GOOGLE_* / EXPO_PUBLIC_GITHUB_CLIENT_ID in
          app/.env.local — see app/.env.example.
        </Text>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: Spacing.xl,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: Palette.bg,
  },
  title: { fontSize: 32, fontWeight: 'bold', marginBottom: Spacing.md },
  subtitle: {
    fontSize: 16,
    color: Palette.textSecondary,
    marginBottom: 50,
    textAlign: 'center',
  },
  googleButton: {
    backgroundColor: '#4285F4', // Google brand blue (intentionally not Palette.primary)
    paddingVertical: 15,
    paddingHorizontal: Spacing.xl,
    borderRadius: Radii.button,
    width: '100%',
    alignItems: 'center',
    marginBottom: Spacing.md,
  },
  googleButtonText: { color: Palette.white, fontSize: 16, fontWeight: 'bold' },
  githubButton: {
    backgroundColor: '#24292e', // GitHub brand near-black
    paddingVertical: 15,
    paddingHorizontal: Spacing.xl,
    borderRadius: Radii.button,
    width: '100%',
    alignItems: 'center',
  },
  githubButtonText: { color: Palette.white, fontSize: 16, fontWeight: 'bold' },
  buttonDisabled: { opacity: 0.45 },
  notConfigured: { color: Palette.white, fontSize: 11, marginTop: 4, opacity: 0.85 },
  helpText: {
    fontSize: 11,
    color: Palette.textTertiary,
    textAlign: 'center',
    marginTop: Spacing.xl,
  },
});
