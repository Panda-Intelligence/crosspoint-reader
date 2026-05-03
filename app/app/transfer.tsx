import React, { useState } from 'react';
import { StyleSheet, Text, View, TouchableOpacity, ActivityIndicator, Alert } from 'react-native';
import * as DocumentPicker from 'expo-document-picker';
import { TransferApi } from '../src/api/transfer';
import { useRouter } from 'expo-router';

export default function TransferScreen() {
  const [uploading, setUploading] = useState(false);
  const [progress, setProgress] = useState(0);
  const router = useRouter();

  const pickAndUploadFile = async () => {
    try {
      const result = await DocumentPicker.getDocumentAsync({
        type: ['application/epub+zip', 'text/plain', 'application/zip'], // EPUB, TXT, CBZ (ZIP)
        copyToCacheDirectory: true,
      });

      if (result.canceled || !result.assets || result.assets.length === 0) {
        return;
      }

      const file = result.assets[0];
      
      setUploading(true);
      setProgress(0);

      await TransferApi.uploadFile(
        {
          uri: file.uri,
          name: file.name,
          type: file.mimeType || 'application/octet-stream',
        },
        (p) => setProgress(p)
      );

      setUploading(false);
      Alert.alert('Success', `${file.name} has been uploaded to your Reader.`);
    } catch (error: any) {
      setUploading(false);
      Alert.alert('Upload Failed', error.message || 'Check your connection to the Reader.');
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>File Transfer</Text>
      <Text style={styles.subtitle}>Send EPUB, TXT, or CBZ files to your device</Text>

      <TouchableOpacity 
        style={[styles.button, uploading && styles.buttonDisabled]} 
        onPress={pickAndUploadFile}
        disabled={uploading}
      >
        {uploading ? (
          <ActivityIndicator color="#fff" />
        ) : (
          <Text style={styles.buttonText}>Pick & Send File</Text>
        )}
      </TouchableOpacity>

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
    justifyContent: 'center',
    alignItems: 'center',
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
    marginBottom: 10,
  },
  subtitle: {
    fontSize: 16,
    color: '#666',
    textAlign: 'center',
    marginBottom: 40,
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
    marginTop: 30,
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
    marginTop: 40,
  },
  backButtonText: {
    color: '#007AFF',
    fontSize: 16,
  },
});
