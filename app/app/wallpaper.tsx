import React, { useState } from 'react';
import { StyleSheet, Text, View, TouchableOpacity, ActivityIndicator, Alert, Image } from 'react-native';
import * as ImagePicker from 'expo-image-picker';
import { TransferApi } from '../src/api/transfer';
import { useRouter } from 'expo-router';

export default function WallpaperScreen() {
  const [image, setImage] = useState<string | null>(null);
  const [uploading, setUploading] = useState(false);
  const [progress, setProgress] = useState(0);
  const router = useRouter();

  const pickImage = async () => {
    // No permissions request is necessary for launching the image library
    let result = await ImagePicker.launchImageLibraryAsync({
      mediaTypes: ImagePicker.MediaTypeOptions.Images,
      allowsEditing: true,
      aspect: [3, 5], // Approximate for 480x800
      quality: 1, // Full quality, resize on ESP32 side or later here
    });

    if (!result.canceled && result.assets && result.assets.length > 0) {
      setImage(result.assets[0].uri);
    }
  };

  const uploadWallpaper = async () => {
    if (!image) return;

    setUploading(true);
    setProgress(0);

    try {
      await TransferApi.uploadFile(
        {
          uri: image,
          name: `wallpaper_${Date.now()}.jpg`,
          type: 'image/jpeg',
        },
        (p) => setProgress(p)
      );

      Alert.alert('Success', 'Wallpaper sent to Reader. You can apply it from the device settings.');
    } catch (error: any) {
      Alert.alert('Upload Failed', error.message || 'Check your connection.');
    } finally {
      setUploading(false);
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Send Wallpaper</Text>
      
      {image ? (
        <Image source={{ uri: image }} style={styles.previewImage} />
      ) : (
        <View style={styles.placeholderContainer}>
          <Text style={styles.placeholderText}>No image selected</Text>
        </View>
      )}

      <TouchableOpacity style={styles.buttonOutline} onPress={pickImage} disabled={uploading}>
        <Text style={styles.buttonOutlineText}>{image ? 'Choose Different Image' : 'Pick from Gallery'}</Text>
      </TouchableOpacity>

      {image && (
        <TouchableOpacity 
          style={[styles.button, uploading && styles.buttonDisabled]} 
          onPress={uploadWallpaper}
          disabled={uploading}
        >
          {uploading ? (
            <ActivityIndicator color="#fff" />
          ) : (
            <Text style={styles.buttonText}>Send to Device</Text>
          )}
        </TouchableOpacity>
      )}

      {uploading && (
        <View style={styles.progressContainer}>
          <Text style={styles.progressText}>Uploading: {progress}%</Text>
          <View style={styles.progressBar}>
            <View style={[styles.progressFill, { width: `${progress}%` }]} />
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
    marginBottom: 20,
    resizeMode: 'cover',
  },
  placeholderContainer: {
    width: 240,
    height: 400,
    borderRadius: 10,
    backgroundColor: '#f0f0f0',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 20,
    borderWidth: 1,
    borderColor: '#ddd',
    borderStyle: 'dashed',
  },
  placeholderText: {
    color: '#999',
  },
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
  buttonOutlineText: {
    color: '#007AFF',
    fontSize: 18,
    fontWeight: '600',
  },
  button: {
    backgroundColor: '#007AFF',
    paddingVertical: 15,
    paddingHorizontal: 30,
    borderRadius: 10,
    width: '100%',
    alignItems: 'center',
  },
  buttonDisabled: {
    backgroundColor: '#A0A0A0',
  },
  buttonText: {
    color: '#fff',
    fontSize: 18,
    fontWeight: '600',
  },
  progressContainer: {
    marginTop: 20,
    width: '100%',
  },
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
  progressFill: {
    height: '100%',
    backgroundColor: '#4CD964',
  },
  backButton: {
    marginTop: 30,
  },
  backButtonText: {
    color: '#007AFF',
    fontSize: 16,
  },
});
