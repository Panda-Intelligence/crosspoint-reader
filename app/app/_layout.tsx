import { DarkTheme, DefaultTheme, ThemeProvider } from '@react-navigation/native';
import { Stack, useRouter, useSegments } from 'expo-router';
import { StatusBar } from 'expo-status-bar';
import { useEffect, useRef } from 'react';
import 'react-native-reanimated';

import { useColorScheme } from '@/hooks/use-color-scheme';
import { AuthProvider, useAuth } from '@/src/contexts/AuthContext';
import { setUnauthorizedHandler } from '@/src/api/client';

export const unstable_settings = {
  anchor: '(tabs)',
};

function RootLayoutNav() {
  const colorScheme = useColorScheme();
  const { user, isLoading } = useAuth();
  const segments = useSegments();
  const router = useRouter();
  // Throttle the 401-redirect: a single failing screen with multiple
  // requests in flight would otherwise call replace('/scanner') N times.
  const lastRedirectAt = useRef<number>(0);

  useEffect(() => {
    if (isLoading) return;

    const inAuthGroup = segments[0] === 'login';

    if (!user && !inAuthGroup) {
      router.replace('/login');
    } else if (user && inAuthGroup) {
      router.replace('/(tabs)');
    }
  }, [user, isLoading, segments]);

  // Register a global 401 handler that bounces the user to QR pairing.
  // The Scanner screen re-stores IP + token, which is the only way to
  // recover from "device rotated its api token" or "user wiped the token".
  useEffect(() => {
    setUnauthorizedHandler(() => {
      const now = Date.now();
      if (now - lastRedirectAt.current < 5000) return;
      lastRedirectAt.current = now;
      router.replace('/scanner');
    });
    return () => setUnauthorizedHandler(null);
  }, [router]);

  return (
    <ThemeProvider value={colorScheme === 'dark' ? DarkTheme : DefaultTheme}>
      <Stack>
        <Stack.Screen name="login" options={{ headerShown: false }} />
        <Stack.Screen name="(tabs)" options={{ headerShown: false }} />
      </Stack>
      <StatusBar style="auto" />
    </ThemeProvider>
  );
}

export default function RootLayout() {
  return (
    <AuthProvider>
      <RootLayoutNav />
    </AuthProvider>
  );
}
