import { Image, StyleSheet, Platform, View, TouchableOpacity, Text, Alert } from 'react-native';
import { useRouter } from 'expo-router';
import { useState, useEffect } from 'react';
import { useIsFocused } from '@react-navigation/native';
import { DeviceStorage } from '../../src/utils/storage';
import { useAuth } from '../../src/contexts/AuthContext';

import { HelloWave } from '@/components/hello-wave';
import ParallaxScrollView from '@/components/parallax-scroll-view';
import { ThemedText } from '@/components/themed-text';
import { ThemedView } from '@/components/themed-view';

export default function HomeScreen() {
  const router = useRouter();
  const [deviceIp, setDeviceIp] = useState<string | null>(null);
  const isFocused = useIsFocused();
  const { logout, user } = useAuth();

  useEffect(() => {
    if (isFocused) {
      DeviceStorage.getIP().then(setDeviceIp);
    }
  }, [isFocused]);

  const handleDisconnect = async () => {
    Alert.alert('Disconnect Device', 'Are you sure you want to unbind from the current reader?', [
      { text: 'Cancel', style: 'cancel' },
      { text: 'Unbind', style: 'destructive', onPress: async () => {
          await DeviceStorage.clearDevice();
          setDeviceIp(null);
        }
      }
    ]);
  };

  const handleLogout = async () => {
    await logout();
  };

  return (
    <ParallaxScrollView
      headerBackgroundColor={{ light: '#A1CEDC', dark: '#1D3D47' }}
      headerImage={
        <Image
          source={require('@/assets/images/partial-react-logo.png')}
          style={styles.reactLogo}
        />
      }>
      <ThemedView style={styles.titleContainer}>
        <ThemedText type="title">Murphy Mate</ThemedText>
        <HelloWave />
      </ThemedView>
      <ThemedText>Hi, {user?.name || 'User'}!</ThemedText>
      
      <ThemedView style={styles.stepContainer}>
        <ThemedText type="subtitle">Step 1: Connect</ThemedText>
        {deviceIp ? (
          <View style={styles.statusBox}>
            <Text style={styles.statusText}>Connected to: {deviceIp}</Text>
            <TouchableOpacity style={styles.disconnectButton} onPress={handleDisconnect}
              accessibilityRole="button"
              accessibilityLabel="Unbind paired device">
              <Text style={styles.disconnectText}>Unbind</Text>
            </TouchableOpacity>
          </View>
        ) : (
          <>
            <ThemedText>
              Scan the QR code on your Crosspoint Reader to establish a secure local connection.
            </ThemedText>
            <TouchableOpacity
              style={styles.connectButton}
              onPress={() => router.push('/scanner')}
              accessibilityRole="button"
              accessibilityLabel="Scan QR code to pair device"
            >
              <Text style={styles.buttonText}>Scan QR Code</Text>
            </TouchableOpacity>
            <TouchableOpacity
              style={styles.discoverButton}
              onPress={() => router.push('/discover')}
              accessibilityRole="button"
              accessibilityLabel="Discover Crosspoint Reader on local Wi-Fi"
            >
              <Text style={styles.discoverButtonText}>Discover on Wi-Fi</Text>
            </TouchableOpacity>
          </>
        )}
      </ThemedView>

      <ThemedView style={styles.stepContainer}>
        <ThemedText type="subtitle">Step 2: Transfer</ThemedText>
        <ThemedText>
          Send books (EPUB, TXT) or images directly to your device.
        </ThemedText>
        <TouchableOpacity
          style={[styles.transferButton, !deviceIp && styles.disabledButton]}
          disabled={!deviceIp}
          onPress={() => router.push('/transfer')}
          accessibilityRole="button"
          accessibilityLabel="Send files to device"
          accessibilityState={{ disabled: !deviceIp }}
        >
          <Text style={styles.buttonText}>Send Files</Text>
        </TouchableOpacity>
      </ThemedView>

      <ThemedView style={styles.stepContainer}>
        <ThemedText type="subtitle">Step 3: Sync Settings</ThemedText>
        <ThemedText>
          Manage device settings and reading preferences from your phone.
        </ThemedText>
        <TouchableOpacity
          style={[styles.settingsButton, !deviceIp && styles.disabledButton]}
          disabled={!deviceIp}
          onPress={() => router.push('/settings')}
          accessibilityRole="button"
          accessibilityLabel="Edit device settings"
          accessibilityState={{ disabled: !deviceIp }}
        >
          <Text style={styles.buttonText}>Edit Settings</Text>
        </TouchableOpacity>
      </ThemedView>

      <ThemedView style={styles.stepContainer}>
        <ThemedText type="subtitle">Step 4: Personalize</ThemedText>
        <ThemedText>
          Set a custom wallpaper or sleep screen for your Reader.
        </ThemedText>
        <TouchableOpacity
          style={[styles.wallpaperButton, !deviceIp && styles.disabledButton]}
          disabled={!deviceIp}
          onPress={() => router.push('/wallpaper')}
          accessibilityRole="button"
          accessibilityLabel="Send wallpaper to device"
          accessibilityState={{ disabled: !deviceIp }}
        >
          <Text style={styles.buttonText}>Send Wallpaper</Text>
        </TouchableOpacity>
      </ThemedView>

      <TouchableOpacity
        style={styles.logoutButton}
        onPress={handleLogout}
        accessibilityRole="button"
        accessibilityLabel="Log out of Murphy Mate">
        <Text style={styles.logoutText}>Logout</Text>
      </TouchableOpacity>
    </ParallaxScrollView>
  );
}

const styles = StyleSheet.create({
  titleContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  stepContainer: {
    gap: 8,
    marginBottom: 8,
  },
  reactLogo: {
    height: 178,
    width: 290,
    bottom: 0,
    left: 0,
    position: 'absolute',
  },
  connectButton: {
    backgroundColor: '#007AFF',
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 8,
  },
  discoverButton: {
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 6,
    borderWidth: 1,
    borderColor: '#007AFF',
  },
  discoverButtonText: {
    color: '#007AFF',
    fontWeight: '600',
    fontSize: 16,
  },
  transferButton: {
    backgroundColor: '#34C759',
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 8,
  },
  settingsButton: {
    backgroundColor: '#FF9500',
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 8,
  },
  wallpaperButton: {
    backgroundColor: '#AF52DE',
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 8,
  },
  disabledButton: {
    opacity: 0.5,
  },
  buttonText: {
    color: '#fff',
    fontWeight: '600',
    fontSize: 16,
  },
  statusBox: {
    backgroundColor: '#e6f4fe',
    padding: 15,
    borderRadius: 8,
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginTop: 5,
  },
  statusText: {
    color: '#007AFF',
    fontWeight: '500',
  },
  disconnectButton: {
    padding: 8,
    backgroundColor: '#ff3b30',
    borderRadius: 6,
  },
  disconnectText: {
    color: '#fff',
    fontWeight: 'bold',
    fontSize: 12,
  },
  logoutButton: {
    marginTop: 20,
    padding: 15,
    alignItems: 'center',
  },
  logoutText: {
    color: '#ff3b30',
    fontWeight: '600',
  }
});
