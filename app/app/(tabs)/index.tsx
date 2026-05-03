import { Image, StyleSheet, Platform, View, TouchableOpacity, Text } from 'react-native';
import { useRouter } from 'expo-router';

import { HelloWave } from '@/components/hello-wave';
import ParallaxScrollView from '@/components/parallax-scroll-view';
import { ThemedText } from '@/components/themed-text';
import { ThemedView } from '@/components/themed-view';

export default function HomeScreen() {
  const router = useRouter();

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
      
      <ThemedView style={styles.stepContainer}>
        <ThemedText type="subtitle">Step 1: Connect</ThemedText>
        <ThemedText>
          Scan the QR code on your Crosspoint Reader to establish a secure local connection.
        </ThemedText>
        <TouchableOpacity 
          style={styles.connectButton} 
          onPress={() => router.push('/scanner')}
        >
          <Text style={styles.buttonText}>Scan QR Code</Text>
        </TouchableOpacity>
      </ThemedView>

      <ThemedView style={styles.stepContainer}>
        <ThemedText type="subtitle">Step 2: Transfer</ThemedText>
        <ThemedText>
          Send books (EPUB, TXT) or images directly to your device.
        </ThemedText>
        <TouchableOpacity 
          style={styles.transferButton} 
          onPress={() => router.push('/transfer')}
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
          style={styles.settingsButton} 
          onPress={() => router.push('/settings')}
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
          style={styles.wallpaperButton} 
          onPress={() => router.push('/wallpaper')}
        >
          <Text style={styles.buttonText}>Send Wallpaper</Text>
        </TouchableOpacity>
      </ThemedView>
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
  buttonText: {
    color: '#fff',
    fontWeight: '600',
    fontSize: 16,
  },
});
