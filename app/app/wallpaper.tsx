import React, { useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  Image,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import * as ImagePicker from 'expo-image-picker';
import {
  manipulateAsync,
  SaveFormat,
  type ImageResult,
} from 'expo-image-manipulator';
import { useRouter } from 'expo-router';

import { TransferApi } from '../src/api/transfer';

// Crosspoint Reader native panel resolution (portrait, full screen).
const TARGET_WIDTH = 480;
const TARGET_HEIGHT = 800;

type Phase = 'idle' | 'preparing' | 'uploading';

interface PickedImage {
  uri: string;
  width: number;
  height: number;
}

export default function WallpaperScreen() {
  const router = useRouter();
  const [picked, setPicked] = useState<PickedImage | null>(null);
  const [phase, setPhase] = useState<Phase>('idle');
  const [progress, setProgress] = useState(0);

  const pickImage = async () => {
    const result = await ImagePicker.launchImageLibraryAsync({
      mediaTypes: ImagePicker.MediaTypeOptions.Images,
      allowsEditing: true,
      aspect: [3, 5], // approximate 480x800
      quality: 1,
    });

    if (result.canceled || !result.assets || result.assets.length === 0) {
      return;
    }
    const a = result.assets[0];
    setPicked({ uri: a.uri, width: a.width, height: a.height });
  };

  // Resize to fit within TARGET_WIDTH x TARGET_HEIGHT preserving aspect.
  // Returns the original URI unchanged if both dimensions already fit.
  const preprocess = async (img: PickedImage): Promise<PickedImage> => {
    if (img.width <= TARGET_WIDTH && img.height <= TARGET_HEIGHT) {
      return img;
    }
    const scale = Math.min(TARGET_WIDTH / img.width, TARGET_HEIGHT / img.height);
    const newWidth = Math.round(img.width * scale);
    const newHeight = Math.round(img.height * scale);
    const out: ImageResult = await manipulateAsync(
      img.uri,
      [{ resize: { width: newWidth, height: newHeight } }],
      { compress: 0.8, format: SaveFormat.JPEG },
    );
    return { uri: out.uri, width: out.width, height: out.height };
  };

  const uploadWallpaper = async () => {
    if (!picked) return;
    try {
      setPhase('preparing');
      setProgress(0);
      const prepared = await preprocess(picked);

      setPhase('uploading');
      await TransferApi.uploadFile(
        {
          uri: prepared.uri,
          name: `wallpaper_${Date.now()}.jpg`,
          type: 'image/jpeg',
        },
        (p) => setProgress(p),
      );

      Alert.alert(
        'Success',
        `Wallpaper sent (${prepared.width}×${prepared.height}). Apply it from device settings.`,
      );
    } catch (error: any) {
      Alert.alert('Upload Failed', error?.message ?? 'Check your connection.');
    } finally {
      setPhase('idle');
    }
  };

  const busy = phase !== 'idle';
  const phaseLabel =
    phase === 'preparing'
      ? 'Preparing image…'
      : phase === 'uploading'
      ? `Uploading: ${progress}%`
      : '';

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Send Wallpaper</Text>

      {picked ? (
        <Image source={{ uri: picked.uri }} style={styles.previewImage} />
      ) : (
        <View style={styles.placeholderContainer}>
          <Text style={styles.placeholderText}>No image selected</Text>
        </View>
      )}

      {picked && (
        <Text style={styles.dimText}>
          Source: {picked.width}×{picked.height}
          {(picked.width > TARGET_WIDTH || picked.height > TARGET_HEIGHT)
            ? `  →  will resize to fit ${TARGET_WIDTH}×${TARGET_HEIGHT}`
            : '  (no resize needed)'}
        </Text>
      )}

      <TouchableOpacity
        style={styles.buttonOutline}
        onPress={pickImage}
        disabled={busy}
      >
        <Text style={styles.buttonOutlineText}>
          {picked ? 'Choose Different Image' : 'Pick from Gallery'}
        </Text>
      </TouchableOpacity>

      {picked && (
        <TouchableOpacity
          style={[styles.button, busy && styles.buttonDisabled]}
          onPress={uploadWallpaper}
          disabled={busy}
        >
          {busy ? (
            <ActivityIndicator color="#fff" />
          ) : (
            <Text style={styles.buttonText}>Send to Device</Text>
          )}
        </TouchableOpacity>
      )}

      {busy && (
        <View style={styles.progressContainer}>
          <Text style={styles.progressText}>{phaseLabel}</Text>
          <View style={styles.progressBar}>
            <View
              style={[
                styles.progressFill,
                { width: `${phase === 'uploading' ? progress : 0}%` },
              ]}
            />
          </View>
        </View>
      )}

      <TouchableOpacity style={styles.backButton} onPress={() => router.back()}>
        <Text style={styles.backButtonText}>Go Back</Text>
      </TouchableOpacity>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 20,
    backgroundColor: '#fff',
    alignItems: 'center',
    paddingTop: 60,
  },
  title: {
    fontSize: 28,
    fontWeight: 'bold',
    marginBottom: 30,
  },
  previewImage: {
    width: 240,
    height: 400,
    borderRadius: 10,
    marginBottom: 12,
    resizeMode: 'cover',
  },
  placeholderContainer: {
    width: 240,
    height: 400,
    borderRadius: 10,
    backgroundColor: '#f0f0f0',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 12,
    borderWidth: 1,
    borderColor: '#ddd',
    borderStyle: 'dashed',
  },
  placeholderText: { color: '#999' },
  dimText: { fontSize: 12, color: '#666', marginBottom: 16, textAlign: 'center' },
  buttonOutline: {
    paddingVertical: 15,
    paddingHorizontal: 30,
    borderRadius: 10,
    width: '100%',
    alignItems: 'center',
    borderWidth: 2,
    borderColor: '#007AFF',
    marginBottom: 15,
  },
  buttonOutlineText: { color: '#007AFF', fontSize: 18, fontWeight: '600' },
  button: {
    backgroundColor: '#007AFF',
    paddingVertical: 15,
    paddingHorizontal: 30,
    borderRadius: 10,
    width: '100%',
    alignItems: 'center',
  },
  buttonDisabled: { backgroundColor: '#A0A0A0' },
  buttonText: { color: '#fff', fontSize: 18, fontWeight: '600' },
  progressContainer: { marginTop: 20, width: '100%' },
  progressText: {
    textAlign: 'center',
    marginBottom: 10,
    fontSize: 14,
    color: '#333',
  },
  progressBar: {
    height: 10,
    backgroundColor: '#E0E0E0',
    borderRadius: 5,
    overflow: 'hidden',
  },
  progressFill: { height: '100%', backgroundColor: '#4CD964' },
  backButton: { marginTop: 30 },
  backButtonText: { color: '#007AFF', fontSize: 16 },
});
