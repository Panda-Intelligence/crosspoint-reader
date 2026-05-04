import React, { useEffect } from 'react';
import {
  StyleSheet,
  Text,
  View,
  TouchableOpacity,
  Alert,
} from 'react-native';
import { useRouter } from 'expo-router';
import * as WebBrowser from 'expo-web-browser';
import * as Google from 'expo-auth-session/providers/google';
import * as AuthSession from 'expo-auth-session';

import { useAuth, User } from '../src/contexts/AuthContext';

WebBrowser.maybeCompleteAuthSession();

// TODO: Replace with real GitHub OAuth App client_id before shipping.
// Bundle ID must match: ai.pandacat.app.murphy.mate.
// PKCE is enabled, so no client_secret is bundled in the app.
// Configure the OAuth App with the redirect URI printed by makeRedirectUri()
// (see console output on first run, e.g. murphymate://auth/github).
const GITHUB_CLIENT_ID = 'YOUR_GITHUB_CLIENT_ID_HERE';
const GITHUB_DISCOVERY: AuthSession.DiscoveryDocument = {
  authorizationEndpoint: 'https://github.com/login/oauth/authorize',
  tokenEndpoint: 'https://github.com/login/oauth/access_token',
  revocationEndpoint: `https://github.com/settings/connections/applications/${GITHUB_CLIENT_ID}`,
};

export default function LoginScreen() {
  const { login } = useAuth();
  const router = useRouter();

  // --- Google ---
  // NOTE: Replace with real client IDs from Google Cloud Console.
  const [googleRequest, googleResponse, googlePromptAsync] = Google.useAuthRequest({
    iosClientId: 'YOUR_IOS_CLIENT_ID_HERE.apps.googleusercontent.com',
    androidClientId: 'YOUR_ANDROID_CLIENT_ID_HERE.apps.googleusercontent.com',
    webClientId: 'YOUR_WEB_CLIENT_ID_HERE.apps.googleusercontent.com',
  });

  useEffect(() => {
    if (googleResponse?.type === 'success') {
      void fetchGoogleUser(googleResponse.authentication?.accessToken);
    }
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
        clientId: GITHUB_CLIENT_ID,
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

      // GitHub may not include the primary email in /user; query /user/emails.
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
        style={styles.googleButton}
        disabled={!googleRequest}
        onPress={() => googlePromptAsync()}
      >
        <Text style={styles.googleButtonText}>Sign in with Google</Text>
      </TouchableOpacity>

      <TouchableOpacity
        style={styles.githubButton}
        disabled={!githubRequest}
        onPress={() => githubPromptAsync()}
      >
        <Text style={styles.githubButtonText}>Sign in with GitHub</Text>
      </TouchableOpacity>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 20,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#fff',
  },
  title: {
    fontSize: 32,
    fontWeight: 'bold',
    marginBottom: 10,
  },
  subtitle: {
    fontSize: 16,
    color: '#666',
    marginBottom: 50,
    textAlign: 'center',
  },
  googleButton: {
    backgroundColor: '#4285F4',
    paddingVertical: 15,
    paddingHorizontal: 20,
    borderRadius: 8,
    width: '100%',
    alignItems: 'center',
    marginBottom: 15,
  },
  googleButtonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
  },
  githubButton: {
    backgroundColor: '#24292e',
    paddingVertical: 15,
    paddingHorizontal: 20,
    borderRadius: 8,
    width: '100%',
    alignItems: 'center',
  },
  githubButtonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
  },
});
